/************************************************************************
 * File: cvkx.c                                                         *
 * Programmers: Scott D. Cohen, Alan C. Hindmarsh and Radu Serban @LLNL *
 * Version of : 10 July 2003                                            *
 *----------------------------------------------------------------------*
 * Example problem.                                                     *
 * An ODE system is generated from the following 2-species diurnal      *
 * kinetics advection-diffusion PDE system in 2 space dimensions:       *
 *                                                                      *
 * dc(i)/dt = Kh*(d/dx)^2 c(i) + V*dc(i)/dx + (d/dz)(Kv(z)*dc(i)/dz)    *
 *                 + Ri(c1,c2,t)      for i = 1,2,   where              *
 *   R1(c1,c2,t) = -q1*c1*c3 - q2*c1*c2 + 2*q3(t)*c3 + q4(t)*c2 ,       *
 *   R2(c1,c2,t) =  q1*c1*c3 - q2*c1*c2 - q4(t)*c2 ,                    *
 *   Kv(z) = Kv0*exp(z/5) ,                                             *
 * Kh, V, Kv0, q1, q2, and c3 are constants, and q3(t) and q4(t)        *
 * vary diurnally.   The problem is posed on the square                 *
 *   0 <= x <= 20,    30 <= z <= 50   (all in km),                      *
 * with homogeneous Neumann boundary conditions, and for time t in      *
 *   0 <= t <= 86400 sec (1 day).                                       *
 * The PDE system is treated by central differences on a uniform        *
 * 10 x 10 mesh, with simple polynomial initial profiles.               *
 * The problem is solved with CVODE, with the BDF/GMRES method (i.e.    *
 * using the CVSPGMR linear solver) and the block-diagonal part of the  *
 * Newton matrix as a left preconditioner. A copy of the block-diagonal *
 * part of the Jacobian is saved and conditionally reused within the    *
 * Precond routine.                                                     *
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "sundialstypes.h"  /* definitions of realtype, integertype           */
#include "cvodes.h"         /* main CVODE header file                         */
#include "iterativ.h"       /* contains the enum for types of preconditioning */
#include "cvsspgmr.h"       /* use CVSPGMR linear solver each internal step   */
#include "smalldense.h"     /* use generic DENSE solver for preconditioning   */
#include "nvector_serial.h" /* definitions of type N_Vector, macro NV_DATA_S  */
#include "sundialsmath.h"   /* contains SQR macro                             */


/* Problem Constants */

#define NUM_SPECIES  2             /* number of species         */
#define KH           4.0e-6        /* horizontal diffusivity Kh */
#define VEL          0.001         /* advection velocity V      */
#define KV0          1.0e-8        /* coefficient in Kv(z)      */
#define Q1           1.63e-16      /* coefficients q1, q2, c3   */ 
#define Q2           4.66e-16
#define C3           3.7e16
#define A3           22.62         /* coefficient in expression for q3(t) */
#define A4           7.601         /* coefficient in expression for q4(t) */
#define C1_SCALE     1.0e6         /* coefficients in initial profiles    */
#define C2_SCALE     1.0e12

#define T0           0.0           /* initial time */
#define NOUT         12            /* number of output times */
#define TWOHR        7200.0        /* number of seconds in two hours  */
#define HALFDAY      4.32e4        /* number of seconds in a half day */
#define PI       3.1415926535898   /* pi */ 

#define XMIN          0.0          /* grid boundaries in x  */
#define XMAX         20.0           
#define ZMIN         30.0          /* grid boundaries in z  */
#define ZMAX         50.0
#define XMID         10.0          /* grid midpoints in x,z */          
#define ZMID         40.0

#define MX           10             /* MX = number of x mesh points */
#define MZ           10             /* MZ = number of z mesh points */
#define NSMX         20             /* NSMX = NUM_SPECIES*MX */
#define MM           (MX*MZ)        /* MM = MX*MZ */

/* CVodeMalloc Constants */

#define RTOL    1.0e-5            /* scalar relative tolerance */
#define FLOOR   100.0             /* value of C1 or C2 at which tolerances */
                                  /* change from relative to absolute      */
#define ATOL    (RTOL*FLOOR)      /* scalar absolute tolerance */
#define NEQ     (NUM_SPECIES*MM)  /* NEQ = number of equations */


/* User-defined vector and matrix accessor macros: IJKth, IJth */

/* IJKth is defined in order to isolate the translation from the
   mathematical 3-dimensional structure of the dependent variable vector
   to the underlying 1-dimensional storage. IJth is defined in order to
   write code which indexes into small dense matrices with a (row,column)
   pair, where 1 <= row, column <= NUM_SPECIES.   
   
   IJKth(vdata,i,j,k) references the element in the vdata array for
   species i at mesh point (j,k), where 1 <= i <= NUM_SPECIES,
   0 <= j <= MX-1, 0 <= k <= MZ-1. The vdata array is obtained via
   the macro call vdata = NV_DATA_S(v), where v is an N_Vector. 
   For each mesh point (j,k), the elements for species i and i+1 are
   contiguous within vdata.

   IJth(a,i,j) references the (i,j)th entry of the small matrix realtype **a,
   where 1 <= i,j <= NUM_SPECIES. The small matrix routines in dense.h
   work with matrices stored by column in a 2-dimensional array. In C,
   arrays are indexed starting at 0, not 1. */


#define IJKth(vdata,i,j,k) (vdata[i-1 + (j)*NUM_SPECIES + (k)*NSMX])
#define IJth(a,i,j)        (a[j-1][i-1])


/* Type : UserData 
   contains preconditioner blocks, pivot arrays, and problem constants */

typedef struct {
  realtype **P[MX][MZ], **Jbd[MX][MZ];
  integertype *pivot[MX][MZ];
  realtype q4, om, dx, dz, hdco, haco, vdco;
} *UserData;


/* Private Helper Functions */

static UserData AllocUserData(void);
static void InitUserData(UserData data);
static void FreeUserData(UserData data);
static void SetInitialProfiles(N_Vector y, realtype dx, realtype dz);
static void PrintOutput(void *cvode_mem, N_Vector y, realtype t);
static void PrintFinalStats(void *cvode_mem);

/* Functions Called by the CVODE Solver */

static void f(realtype t, N_Vector y, N_Vector ydot, void *f_data);

static int Precond(realtype tn, N_Vector y, N_Vector fy,
                   booleantype jok, booleantype *jcurPtr, realtype gamma,
                   void *P_data, N_Vector vtemp1, N_Vector vtemp2,
                   N_Vector vtemp3);

static int PSolve(realtype tn, N_Vector y, N_Vector fy,
                  N_Vector r, N_Vector z,
                  realtype gamma, realtype delta,
                  int lr, void *P_data, N_Vector vtemp);


/***************************** Main Program ******************************/

int main()
{
  NV_Spec nvSpec;
  realtype abstol, reltol, t, tout;
  N_Vector y;
  UserData data;
  void *cvode_mem;
  int iout, flag;

  /* Initialize serial vector specification object */
  nvSpec = NV_SpecInit_Serial(NEQ);

  /* Allocate memory, and set problem data, initial values, tolerances */ 
  y = N_VNew(nvSpec);
  data = AllocUserData();
  InitUserData(data);
  SetInitialProfiles(y, data->dx, data->dz);
  abstol=ATOL; 
  reltol=RTOL;

  /* Call CvodeCreate to create CVODES memory 

     BDF     specifies the Backward Differentiation Formula
     NEWTON  specifies a Newton iteration

     A pointer to CVODES problem memory is returned and stored in cvode_mem. */
  cvode_mem = CVodeCreate(BDF, NEWTON);
  if (cvode_mem == NULL) { printf("CVodeCreate failed.\n"); return(1); }

  /* Set the pointer to user-defined data */
  flag = CVodeSetFdata(cvode_mem, data);
  if (flag != SUCCESS) { printf("CVodeSetFdata failed.\n"); return(1); }

  /* Call CVodeMalloc to initialize CVODES memory: 

     f       is the user's right hand side function in y'=f(t,y)
     T0      is the initial time
     y       is the initial dependent variable vector
     SS      specifies scalar relative and absolute tolerances
     &reltol and &abstol are pointers to the scalar tolerances      */
  flag = CVodeMalloc(cvode_mem, f, T0, y, SS, &reltol, &abstol, nvSpec);
  if (flag != SUCCESS) { printf("CVodeMalloc failed.\n"); return(1); }

  /* Call CVSpgmr to specify the CVODES linear solver CVSPGMR 
     with left preconditioning and the maximum Krylov dimension maxl */
  flag = CVSpgmr(cvode_mem, LEFT, 0);
  if (flag != SUCCESS) { printf("CVSpgmr failed."); return(1); }

  /* Set modified Gram-Schmidt orthogonalization, preconditioner 
     setup and solve routines Precond and PSolve, and the pointer 
     to the user-defined block data */
  flag = CVSpgmrSetGSType(cvode_mem, MODIFIED_GS);
  flag = CVSpgmrSetPrecSetupFn(cvode_mem, Precond);
  flag = CVSpgmrSetPrecSolveFn(cvode_mem, PSolve);
  flag = CVSpgmrSetPrecData(cvode_mem, data);

  /* In loop over output points, call CVode, print results, test for error */
  printf(" \n2-species diurnal advection-diffusion problem\n\n");
  for (iout=1, tout = TWOHR; iout <= NOUT; iout++, tout += TWOHR) {
    flag = CVode(cvode_mem, tout, y, &t, NORMAL);
    PrintOutput(cvode_mem, y, t);
    if (flag != SUCCESS) { printf("CVode failed, flag=%d.\n", flag); break; }
  }

  PrintFinalStats(cvode_mem);

  /* Free memory */
  N_VFree(y);
  FreeUserData(data);
  CVodeFree(cvode_mem);
  NV_SpecFree_Serial(nvSpec);

  return(0);
}


/*********************** Private Helper Functions ************************/

/* Allocate memory for data structure of type UserData */

static UserData AllocUserData(void)
{
  int jx, jz;
  UserData data;

  data = (UserData) malloc(sizeof *data);

  for (jx=0; jx < MX; jx++) {
    for (jz=0; jz < MZ; jz++) {
      (data->P)[jx][jz] = denalloc(NUM_SPECIES);
      (data->Jbd)[jx][jz] = denalloc(NUM_SPECIES);
      (data->pivot)[jx][jz] = denallocpiv(NUM_SPECIES);
    }
  }

  return(data);
}

/* Load problem constants in data */

static void InitUserData(UserData data)
{
  data->om = PI/HALFDAY;
  data->dx = (XMAX-XMIN)/(MX-1);
  data->dz = (ZMAX-ZMIN)/(MZ-1);
  data->hdco = KH/SQR(data->dx);
  data->haco = VEL/(2.0*data->dx);
  data->vdco = (1.0/SQR(data->dz))*KV0;
}

/* Free data memory */

static void FreeUserData(UserData data)
{
  int jx, jz;

  for (jx=0; jx < MX; jx++) {
    for (jz=0; jz < MZ; jz++) {
      denfree((data->P)[jx][jz]);
      denfree((data->Jbd)[jx][jz]);
      denfreepiv((data->pivot)[jx][jz]);
    }
  }

  free(data);
}

/* Set initial conditions in y */

static void SetInitialProfiles(N_Vector y, realtype dx, realtype dz)
{
  int jx, jz;
  realtype x, z, cx, cz;
  realtype *ydata;

  /* Set pointer to data array in vector y. */

  ydata = NV_DATA_S(y);

  /* Load initial profiles of c1 and c2 into y vector */

  for (jz=0; jz < MZ; jz++) {
    z = ZMIN + jz*dz;
    cz = SQR(0.1*(z - ZMID));
    cz = 1.0 - cz + 0.5*SQR(cz);
    for (jx=0; jx < MX; jx++) {
      x = XMIN + jx*dx;
      cx = SQR(0.1*(x - XMID));
      cx = 1.0 - cx + 0.5*SQR(cx);
      IJKth(ydata,1,jx,jz) = C1_SCALE*cx*cz; 
      IJKth(ydata,2,jx,jz) = C2_SCALE*cx*cz;
    }
  }
}

/* Print current t, step count, order, stepsize, and sampled c1,c2 values */

static void PrintOutput(void *cvode_mem, N_Vector y,realtype t)
{
  int nst, qu;
  realtype hu, *ydata;
  int mxh = MX/2 - 1, mzh = MZ/2 - 1, mx1 = MX - 1, mz1 = MZ - 1;

  ydata = NV_DATA_S(y);

  CVodeGetNumSteps(cvode_mem, &nst);
  CVodeGetLastOrder(cvode_mem, &qu);
  CVodeGetLastStep(cvode_mem, &hu);

  printf("t = %.2e   no. steps = %d   order = %d   stepsize = %.2e\n",
         t, nst, qu, hu);
  printf("c1 (bot.left/middle/top rt.) = %12.3e  %12.3e  %12.3e\n",
         IJKth(ydata,1,0,0), IJKth(ydata,1,mxh,mzh), IJKth(ydata,1,mx1,mz1));
  printf("c2 (bot.left/middle/top rt.) = %12.3e  %12.3e  %12.3e\n\n",
         IJKth(ydata,2,0,0), IJKth(ydata,2,mxh,mzh), IJKth(ydata,2,mx1,mz1));
}

/* Print final statistics contained in iopt */

static void PrintFinalStats(void *cvode_mem)
{
  long int lenrw, leniw ;
  long int lenrwSPGMR, leniwSPGMR;
  int nst, nfe, nsetups, nni, ncfn, netf;
  int nli, npe, nps, ncfl, nfeSPGMR;
  

  CVodeGetIntWorkSpace(cvode_mem, &leniw);
  CVodeGetRealWorkSpace(cvode_mem, &lenrw);
  CVodeGetNumSteps(cvode_mem, &nst);
  CVodeGetNumRhsEvals(cvode_mem, &nfe);
  CVodeGetNumLinSolvSetups(cvode_mem, &nsetups);
  CVodeGetNumErrTestFails(cvode_mem, &netf);
  CVodeGetNumNonlinSolvIters(cvode_mem, &nni);
  CVodeGetNumNonlinSolvConvFails(cvode_mem, &ncfn);

  CVSpgmrGetIntWorkSpace(cvode_mem, &leniwSPGMR);
  CVSpgmrGetRealWorkSpace(cvode_mem, &lenrwSPGMR);
  CVSpgmrGetNumLinIters(cvode_mem, &nli);
  CVSpgmrGetNumPrecEvals(cvode_mem, &npe);
  CVSpgmrGetNumPrecSolves(cvode_mem, &nps);
  CVSpgmrGetNumConvFails(cvode_mem, &ncfl);
  CVSpgmrGetNumRhsEvals(cvode_mem, &nfeSPGMR);

  printf("\nFinal Statistics.. \n\n");
  printf("lenrw   = %5ld     leniw = %5ld\n", lenrw, leniw);
  printf("llrw    = %5ld     lliw  = %5ld\n", lenrwSPGMR, leniwSPGMR);
  printf("nst     = %5d\n"                  , nst);
  printf("nfe     = %5d     nfel  = %5d\n"  , nfe, nfeSPGMR);
  printf("nni     = %5d     nli   = %5d\n"  , nni, nli);
  printf("nsetups = %5d     netf  = %5d\n"  , nsetups, netf);
  printf("npe     = %5d     nps   = %5d\n"  , npe, nps);
  printf("ncfn    = %5d     ncfl  = %5d\n\n", ncfn, ncfl);
}


/***************** Functions Called by the CVODE Solver ******************/

/* f routine. Compute f(t,y). */

static void f(realtype t, N_Vector y,N_Vector ydot, void *f_data)
{
  realtype q3, c1, c2, c1dn, c2dn, c1up, c2up, c1lt, c2lt;
  realtype c1rt, c2rt, czdn, czup, hord1, hord2, horad1, horad2;
  realtype qq1, qq2, qq3, qq4, rkin1, rkin2, s, vertd1, vertd2, zdn, zup;
  realtype q4coef, delz, verdco, hordco, horaco;
  realtype *ydata, *dydata;
  int jx, jz, idn, iup, ileft, iright;
  UserData data;

  data = (UserData) f_data;
  ydata = NV_DATA_S(y);
  dydata = NV_DATA_S(ydot);

  /* Set diurnal rate coefficients. */

  s = sin(data->om*t);
  if (s > 0.0) {
    q3 = exp(-A3/s);
    data->q4 = exp(-A4/s);
  } else {
    q3 = 0.0;
    data->q4 = 0.0;
  }

  /* Make local copies of problem variables, for efficiency. */

  q4coef = data->q4;
  delz = data->dz;
  verdco = data->vdco;
  hordco  = data->hdco;
  horaco  = data->haco;

  /* Loop over all grid points. */

  for (jz=0; jz < MZ; jz++) {

    /* Set vertical diffusion coefficients at jz +- 1/2 */

    zdn = ZMIN + (jz - .5)*delz;
    zup = zdn + delz;
    czdn = verdco*exp(0.2*zdn);
    czup = verdco*exp(0.2*zup);
    idn = (jz == 0) ? 1 : -1;
    iup = (jz == MZ-1) ? -1 : 1;
    for (jx=0; jx < MX; jx++) {

      /* Extract c1 and c2, and set kinetic rate terms. */

      c1 = IJKth(ydata,1,jx,jz); 
      c2 = IJKth(ydata,2,jx,jz);
      qq1 = Q1*c1*C3;
      qq2 = Q2*c1*c2;
      qq3 = q3*C3;
      qq4 = q4coef*c2;
      rkin1 = -qq1 - qq2 + 2.0*qq3 + qq4;
      rkin2 = qq1 - qq2 - qq4;

      /* Set vertical diffusion terms. */

      c1dn = IJKth(ydata,1,jx,jz+idn);
      c2dn = IJKth(ydata,2,jx,jz+idn);
      c1up = IJKth(ydata,1,jx,jz+iup);
      c2up = IJKth(ydata,2,jx,jz+iup);
      vertd1 = czup*(c1up - c1) - czdn*(c1 - c1dn);
      vertd2 = czup*(c2up - c2) - czdn*(c2 - c2dn);

      /* Set horizontal diffusion and advection terms. */

      ileft = (jx == 0) ? 1 : -1;
      iright =(jx == MX-1) ? -1 : 1;
      c1lt = IJKth(ydata,1,jx+ileft,jz); 
      c2lt = IJKth(ydata,2,jx+ileft,jz);
      c1rt = IJKth(ydata,1,jx+iright,jz);
      c2rt = IJKth(ydata,2,jx+iright,jz);
      hord1 = hordco*(c1rt - 2.0*c1 + c1lt);
      hord2 = hordco*(c2rt - 2.0*c2 + c2lt);
      horad1 = horaco*(c1rt - c1lt);
      horad2 = horaco*(c2rt - c2lt);

      /* Load all terms into ydot. */

      IJKth(dydata, 1, jx, jz) = vertd1 + hord1 + horad1 + rkin1; 
      IJKth(dydata, 2, jx, jz) = vertd2 + hord2 + horad2 + rkin2;
    }
  }

}

/* Preconditioner setup routine. Generate and preprocess P. */
static int Precond(realtype tn, N_Vector y, N_Vector fy,
                   booleantype jok, booleantype *jcurPtr, realtype gamma,
                   void *P_data, N_Vector vtemp1, N_Vector vtemp2,
                   N_Vector vtemp3)
{
  realtype c1, c2, czdn, czup, diag, zdn, zup, q4coef, delz, verdco, hordco;
  realtype **(*P)[MZ], **(*Jbd)[MZ];
  integertype *(*pivot)[MZ], ier;
  int jx, jz;
  realtype *ydata, **a, **j;
  UserData data;
  
  /* Make local copies of pointers in P_data, and of pointer to y's data */
  
  data = (UserData) P_data;
  P = data->P;
  Jbd = data->Jbd;
  pivot = data->pivot;
  ydata = NV_DATA_S(y);
  
  if (jok) {
    
    /* jok = TRUE: Copy Jbd to P */
    
    for (jz=0; jz < MZ; jz++)
      for (jx=0; jx < MX; jx++)
        dencopy(Jbd[jx][jz], P[jx][jz], NUM_SPECIES);
    
    *jcurPtr = FALSE;
    
  }
  
  else {
    /* jok = FALSE: Generate Jbd from scratch and copy to P */
    
    /* Make local copies of problem variables, for efficiency. */
    
    q4coef = data->q4;
    delz = data->dz;
    verdco = data->vdco;
    hordco  = data->hdco;
    
    /* Compute 2x2 diagonal Jacobian blocks (using q4 values 
       computed on the last f call).  Load into P. */
    
    for (jz=0; jz < MZ; jz++) {
      zdn = ZMIN + (jz - .5)*delz;
      zup = zdn + delz;
      czdn = verdco*exp(0.2*zdn);
      czup = verdco*exp(0.2*zup);
      diag = -(czdn + czup + 2.0*hordco);
      for (jx=0; jx < MX; jx++) {
        c1 = IJKth(ydata,1,jx,jz);
        c2 = IJKth(ydata,2,jx,jz);
        j = Jbd[jx][jz];
        a = P[jx][jz];
        IJth(j,1,1) = (-Q1*C3 - Q2*c2) + diag;
        IJth(j,1,2) = -Q2*c1 + q4coef;
        IJth(j,2,1) = Q1*C3 - Q2*c2;
        IJth(j,2,2) = (-Q2*c1 - q4coef) + diag;
        dencopy(j, a, NUM_SPECIES);
      }
    }
    
    *jcurPtr = TRUE;
    
  }
  
  /* Scale by -gamma */
  
  for (jz=0; jz < MZ; jz++)
    for (jx=0; jx < MX; jx++)
      denscale(-gamma, P[jx][jz], NUM_SPECIES);
  
  /* Add identity matrix and do LU decompositions on blocks in place. */
  
  for (jx=0; jx < MX; jx++) {
    for (jz=0; jz < MZ; jz++) {
      denaddI(P[jx][jz], NUM_SPECIES);
      ier = gefa(P[jx][jz], NUM_SPECIES, pivot[jx][jz]);
      if (ier != 0) return(1);
    }
  }
  
  return(0);
}


/* Preconditioner solve routine */
static int PSolve(realtype tn, N_Vector y, N_Vector fy,
                  N_Vector r, N_Vector z,
                  realtype gamma, realtype delta,
                  int lr, void *P_data, N_Vector vtemp)
{
  realtype **(*P)[MZ];
  integertype *(*pivot)[MZ];
  int jx, jz;
  realtype *zdata, *v;
  UserData data;

  /* Extract the P and pivot arrays from P_data. */

  data = (UserData) P_data;
  P = data->P;
  pivot = data->pivot;
  zdata = NV_DATA_S(z);
  
  N_VScale(1.0, r, z);
  
  /* Solve the block-diagonal system Px = r using LU factors stored
     in P and pivot data in pivot, and return the solution in z. */
  
  for (jx=0; jx < MX; jx++) {
    for (jz=0; jz < MZ; jz++) {
      v = &(IJKth(zdata, 1, jx, jz));
      gesl(P[jx][jz], NUM_SPECIES, pivot[jx][jz], v);
    }
  }

  return(0);
}
