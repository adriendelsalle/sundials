/*******************************************************************
 * File          : cvsbbdpre.h                                     *
 * Programmers   : Michael Wittman, Alan C. Hindmarsh, and         *
 *                 Radu Serban @ LLNL                              *
 * Version of    : 10 July 2003                                    *
 *-----------------------------------------------------------------*
 * Copyright (c) 2002, The Regents of the University of California * 
 * Produced at the Lawrence Livermore National Laboratory          *
 * All rights reserved                                             *
 * For details, see sundials/cvodes/LICENSE                        *
 *-----------------------------------------------------------------*
 * This is the header file for the CVSBBDPRE module, for a         *
 * band-block-diagonal preconditioner, i.e. a block-diagonal       *
 * matrix with banded blocks, for use with CVODES, CVSpgmr, and    *
 * the parallel implementation of the N_Vector module.             *
 *                                                                 *
 * Summary:                                                        *
 *                                                                 *
 * These routines provide a preconditioner matrix for CVODES that  *
 * is block-diagonal with banded blocks.  The blocking corresponds *
 * to the distribution of the dependent variable vector y among    *
 * the processors.  Each preconditioner block is generated from    *
 * the Jacobian of the local part (on the current processor) of a  *
 * given function g(t,y) approximating f(t,y).  The blocks are     *
 * generated by a difference quotient scheme on each processor     *
 * independently.  This scheme utilizes an assumed banded          *
 * structure with given half-bandwidths, mudq and mldq.            *
 * However, the banded Jacobian block kept by the scheme has       *
 * half-bandwiths mukeep and mlkeep, which may be smaller.         *
 *                                                                 *
 * The user's calling program should have the following form:      *
 *                                                                 *
 *   #include "nvector_parallel.h"                                 *
 *   #include "cvsbbdpre.h"                                        *
 *   ...                                                           *
 *   void *p_data;                                                 *
 *   ...                                                           *
 *   nvspec = NV_SpecInit_Parallel(...);                           *
 *   ...                                                           *
 *   cvode_mem = CVodeCreate(...);                                 *
 *   ier = CVodeMalloc(...);                                       *
 *   ...                                                           *
 *   p_data = CVBBDPrecAlloc(cvode_mem, Nlocal, mudq ,mldq,        *
 *                           mukeep, mlkeep, dqrely, gloc, cfn);   *
 *   ...                                                           *
 *   flag = CVSpgmr(cvode_mem, pretype, maxl);                     *
 *   flag = CVSpgmrSetPrecSetupFn(cvode_mem, CVBBDPrecSetup);      *
 *   flag = CVSpgmrSetPrecSolveFn(cvode_mem, CVBBDPrecSolve);      *
 *   flag = CVSpgmrSetPrecData(cvode_mem, p_data);                 *
 *   ...                                                           *
 *   ier = CVode(...);                                             *
 *   ...                                                           *
 *   CVBBDPrecFree(p_data);                                        *
 *   ...                                                           *
 *   CVodeFree(...);                                               *
 *                                                                 *
 *   NV_SpecFree_Parallel(nvspec);                                 *
 *                                                                 *
 *                                                                 *
 * The user-supplied routines required are:                        *
 *                                                                 *
 *   f    = function defining the ODE right-hand side f(t,y).      *
 *                                                                 *
 *   gloc = function defining the approximation g(t,y).            *
 *                                                                 *
 *   cfn  = function to perform communication need for gloc.       *
 *                                                                 *
 *                                                                 *
 * Notes:                                                          *
 *                                                                 *
 * 1) This header file is included by the user for the definition  *
 *    of the CVBBDData type and for needed function prototypes.    *
 *                                                                 *
 * 2) The CVBBDPrecAlloc call includes half-bandwiths mudq and mldq*
 *    to be used in the difference-quotient calculation of the     *
 *    approximate Jacobian.  They need not be the true             *
 *    half-bandwidths of the Jacobian of the local block of g,     *
 *    when smaller values may provide a greater efficiency.        *
 *    Also, the half-bandwidths mukeep and mlkeep of the retained  *
 *    banded approximate Jacobian block may be even smaller,       *
 *    to reduce storage and computation costs further.             *
 *    For all four half-bandwidths, the values need not be the     *
 *    same on every processor.                                     *
 *                                                                 *
 * 3) The actual name of the user's f function is passed to        *
 *    CVodeMalloc, and the names of the user's gloc and cfn        *
 *    functions are passed to CVBBDPrecAlloc.                      *
 *                                                                 *
 * 4) The pointer to the user-defined data block f_data, which is  *
 *    set through CVodeSetFdata is also available to the user in   *
 *    gloc and cfn.                                                *
 *                                                                 *
 * 5) For the CVSpgmr solver, the preconditioning and Gram-Schmidt *
 *    types, pretype and gstype, are left to the user to specify.  *
 *                                                                 *
 * 6) Functions CVBBDPrecSetup and CVBBDPrecSolve are never called *
 *    by the user explicitly.                                      *
 *                                                                 *
 * 7) Optional outputs specific to this module are available by    *
 *    way of routines listed below.  These include work space sizes*
 *    and the cumulative number of gloc calls.  The costs          *
 *    associated with this module also include nsetups banded LU   *
 *    factorizations, nsetups cfn calls, and nps banded            *
 *    backsolve calls, where nsetups and nps are CVODES optional   *
 *    outputs.                                                     *
 *******************************************************************/


#ifdef __cplusplus     /* wrapper to enable C++ usage */
extern "C" {
#endif

#ifndef _cvsbbdpre_h
#define _cvsbbdpre_h

#include "cvodes.h"
#include "sundialstypes.h"
#include "nvector.h"
#include "band.h"


/******************************************************************
 * Type : CVLocalFn                                               *
 *----------------------------------------------------------------*        
 * The user must supply a function g(t,y) which approximates the  *
 * right-hand side function f for the system y'=f(t,y), and which *
 * is computed locally (without inter-processor communication).   *
 * (The case where g is mathematically identical to f is allowed.)*
 * The implementation of this function must have type CVLocalFn.  *
 *                                                                *
 * This function takes as input the local vector size Nlocal, the *
 * independent variable value t, the local real dependent         *
 * variable vector ylocal, and a pointer to the user-defined data *
 * block f_data.  It is to compute the local part of g(t,y) and   *
 * store this in the vector glocal.                               *
 * (Allocation of memory for ylocal and glocal is handled within  *
 * the preconditioner module.)                                    *
 * The f_data parameter is the same as that specified by the user *
 * through the CVodeSetFdata routine.                             *
 * A CVLocalFn gloc does not have a return value.                 *
 ******************************************************************/

typedef void (*CVLocalFn)(integertype Nlocal, realtype t, 
                          N_Vector ylocal, N_Vector glocal, 
                          void *f_data);

/******************************************************************
 * Type : CVCommFn                                                *
 *----------------------------------------------------------------*        
 * The user must supply a function of type CVCommFn which performs*
 * all inter-processor communication necessary to evaluate the    *
 * approximate right-hand side function described above.          *
 *                                                                *
 * This function takes as input the local vector size Nlocal,     *
 * the independent variable value t, the dependent variable       *
 * vector y, and a pointer to the user-defined data block f_data. *
 * The f_data parameter is the same as that specified by the user *
 * through the CVodeSetFdata routine.  The CVCommFn cfn is        *
 * expected to save communicated data in space defined within the *
 * structure f_data.                                              *
 * A CVCommFn cfn does not have a return value.                   *
 *                                                                *
 * Each call to the CVCommFn cfn is preceded by a call to the     *
 * RhsFn f with the same (t,y) arguments.  Thus cfn can omit any  *
 * communications done by f if relevant to the evaluation of g.   *
 ******************************************************************/

typedef void (*CVCommFn)(integertype Nlocal, realtype t, N_Vector y,
                         void *f_data);

 
/*********************** Definition of CVBBDData *****************/

typedef struct {

  /* passed by user to CVBBDPrecAlloc, used by PrecSetup/PrecSolve */
  integertype mudq, mldq, mukeep, mlkeep;
  realtype dqrely;
  CVLocalFn gloc;
  CVCommFn cfn;

  /* set by CVBBDPrecSetup and used by CVBBDPrecSolve */
  BandMat savedJ;
  BandMat savedP;
  integertype *pivots;

  /* set by CVBBDPrecAlloc and used by CVBBDPrecSetup */
  integertype n_local;

  /* available for optional output: */
  integertype rpwsize;
  integertype ipwsize;
  integertype nge;

  /* Pointer to cvode_mem */
  CVodeMem cv_mem;

} *CVBBDPrecData;


/******************************************************************
 * Function : CVBBDPrecAlloc                                      *
 *----------------------------------------------------------------*
 * CVBBDPrecAlloc allocates and initializes a CVBBDData structure *
 * to be passed to CVSpgmr (and used by CVBBDPrecSetup and        *
 * and CVBBDPrecSolve.                                            *
 *                                                                *
 * The parameters of CVBBDPrecAlloc are as follows:               *
 *                                                                *
 * cvode_mem is the pointer to CVODES memory.                     *
 *                                                                *
 * Nlocal  is the length of the local block of the vectors y etc. *
 *         on the current processor.                              *
 *                                                                *
 * mudq, mldq  are the upper and lower half-bandwidths to be used *
 *         in the difference-quotient computation of the local    *
 *         Jacobian block.                                        *
 *                                                                *
 * mukeep, mlkeep  are the upper and lower half-bandwidths of the *
 *         retained banded approximation to the local Jacobian    * 
 *         block.                                                 *
 *                                                                *
 * dqrely  is an optional input.  It is the relative increment    *
 *         in components of y used in the difference quotient     *
 *         approximations.  To specify the default, pass 0.       *
 *         The default is dqrely = sqrt(unit roundoff).           *
 *                                                                *
 * gloc    is the name of the user-supplied function g(t,y) that  *
 *         approximates f and whose local Jacobian blocks are     *
 *         to form the preconditioner.                            *
 *                                                                *
 * cfn     is the name of the user-defined function that performs *
 *         necessary inter-processor communication for the        *
 *         execution of gloc.                                     *
 *                                                                *
 * CVBBDPrecAlloc returns the storage allocated (type *void),     *
 * or NULL if the request for storage cannot be satisfied.        *
 ******************************************************************/

void *CVBBDPrecAlloc(void *cvode_mem, integertype Nlocal, 
                     integertype mudq, integertype mldq, 
                     integertype mukeep, integertype mlkeep, 
                     realtype dqrely,
                     CVLocalFn gloc, CVCommFn cfn);

/******************************************************************
 * Function : CVBBDPrecReInit                                     *
 *----------------------------------------------------------------*
 * CVBBDPrecReInit re-initializes the BBDPRE module when solving a*
 * sequence of problems of the same size with CVSPGMR/CVBBDPRE,   *
 * provided there is no change in Nlocal, mukeep, or mlkeep.      *
 * After solving one problem, and after calling CVodeReInit to    *
 * re-initialize CVODE for a subsequent problem, call             *
 * CVBBDPrecReInit.                                               *
 * Then call CVReInitSpgmr or CVSpgmr if necessary, depending on  *
 * changes made in the CVSpgmr parameters, before calling CVode.  *
 *                                                                *
 * The first argument to CVBBDPrecReInit must be the pointer pdata*
 * that was returned by CVBBDPrecAlloc.  All other arguments have *
 * the same names and meanings as those of CVBBDPrecAlloc.        *
 *                                                                *
 * The return value of CVBBDPrecReInit is 0, indicating success.  *
 ******************************************************************/

int CVBBDPrecReInit(void *p_data, 
                    integertype mudq, integertype mldq,
                    realtype dqrely,
                    CVLocalFn gloc, CVCommFn cfn);

/******************************************************************
 * Function : CVBBDPrecFree                                       *
 *----------------------------------------------------------------*
 * CVBBDPrecFree frees the memory block p_data allocated by the   *
 * call to CVBBDAlloc.                                            *
 ******************************************************************/

void CVBBDPrecFree(void *p_data);

/******************************************************************
 * Function : CVBBDPrecGet*                                       *
 *----------------------------------------------------------------*
 *                                                                *
 ******************************************************************/

int CVBBDPrecGetIntWorkSpace(void *p_data, long int *leniwBBDP);
int CVBBDPrecGetRealWorkSpace(void *p_data, long int *lenrwBBDP);
int CVBBDPrecGetNumGfnEvals(void *p_data, int *ngevalsBBDP);

/* Return values for CVBBDPrecGet* functions */
/* OKAY = 0 */
enum { BBDP_NO_PDATA = -1 };

/*** Prototypes of functions CVBBDPrecSetup and CVBBDPrecSolve ****/
  
int CVBBDPrecSetup(realtype t, N_Vector y, N_Vector fy, 
                   booleantype jok, booleantype *jcurPtr, 
                   realtype gamma, void *p_data, 
                   N_Vector tmp1, N_Vector tmp2, N_Vector tmp3);

int CVBBDPrecSolve(realtype t, N_Vector y, N_Vector fy, 
                   N_Vector r, N_Vector z, 
                   realtype gamma, realtype delta,
                   int lr, void *p_data, N_Vector tmp);


#endif

#ifdef __cplusplus
}
#endif
