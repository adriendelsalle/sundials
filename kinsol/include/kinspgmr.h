/*******************************************************************
 *                                                                 *
 * File          : kinspgmr.h                                      *
 * Programmers   : Allan G Taylor, Alan C. Hindmarsh, and          *
 *                 Radu Serban @ LLNL                              *
 * Version of    : 4 August 2003                                   *
 *-----------------------------------------------------------------*
 * Copyright (c) 2002, The Regents of the University of California * 
 * Produced at the Lawrence Livermore National Laboratory          *
 * All rights reserved                                             *
 * For details, see sundials/kinsol/LICENSE                        *
 *-----------------------------------------------------------------*
 * This is the header file for the KINSol scaled, preconditioned   *
 * GMRES linear solver, KINSpgmr.                                  *
 *                                                                 *
 * Note: The type integertype must be large enough to store the    *
 * value of the linear system size Neq.                            *
 *                                                                 *
 *******************************************************************/

#ifdef __cplusplus     /* wrapper to enable C++ usage */
extern "C" {
#endif
#ifndef _kinspgmr_h
#define _kinspgmr_h

#include <stdio.h>
#include "kinsol.h"
#include "spgmr.h"
#include "sundialstypes.h"
#include "nvector.h"

 
/******************************************************************
 *                                                                *
 * KINSpgmr solver constants                                      *
 *----------------------------------------------------------------*
 * KINSPGMR_MAXL    : default value for the maximum Krylov        *
 *                   dimension is MIN(N, KINSPGMR_MAXL)           *
 ******************************************************************/

#define KINSPGMR_MAXL    10

/* Constants for error returns from KINSpgmr. */

#define KIN_MEM_NULL      -1
#define KINSPGMR_MEM_FAIL -2
#define SPGMR_MEM_FAIL    -3
 
 
/******************************************************************
 *                                                                *           
 * Type : KINSpgmrPrecSetupFn                                     *
 *----------------------------------------------------------------*
 * The user-supplied preconditioner setup function PrecSetup and  *
 * the user-supplied preconditioner solve function PrecSolve      *
 * together must define the right preconditoner matrix P chosen   *
 * so as to provide an easier system for the Krylov solver        *
 * to solve. PrecSetup is called to provide any matrix data       *
 * required by the subsequent call(s) to PrecSolve. The data is   *
 * stored in the memory allocated to P_data and the structuring of*
 * that memory is up to the user.     More specifically,          *
 * the user-supplied preconditioner setup function PrecSetup      *
 * is to evaluate and preprocess any Jacobian-related data        *
 * needed by the preconditioner solve function PrecSolve.         *
 * This might include forming a crude approximate Jacobian,       *
 * and performing an LU factorization on the resulting            *
 * approximation to J.  This function will not be called in       *
 * advance of every call to PrecSolve, but instead will be        *
 * called only as often as necessary to achieve convergence       *
 * within the Newton iteration in KINSol.  If the PrecSolve       *
 * function needs no preparation, the PrecSetup function can be   *
 * NULL.                                                          *
 *                                                                *
 * PrecSetup should not modify the contents of the arrays         *
 * uu or fval as those arrays are used elsewhere in the           *
 * iteration process.                                             *
 *                                                                *
 * Each call to the PrecSetup function is preceded by a call to   *
 * the system function func. Thus the PrecSetup function can use  *
 * any auxiliary data that is computed by the func function and   *
 * saved in a way accessible to PrecSetup.                        *
 *                                                                *
 * The two scaling arrays, fscale and uscale, are provided to the *
 * PrecSetup function for possible use in approximating Jacobian  *
 * data, e.g. by difference quotients.                            *
 * These arrays should also not be altered                        *
 *                                                                *
 * A function PrecSetup must have the prototype given below.      *
 * Its parameters are as follows:                                 *
 *                                                                *
 * uu      an N_Vector giving the current iterate for the system. *
 *                                                                *
 * uscale  an N_Vector giving the diagonal entries of the uu-     *
 *         scaling matrix.                                        *
 *                                                                *
 * fval    an N_Vector giving the current function value          *
 *                                                                *
 * fscale  an N_Vector giving the diagonal entries of the func-   *
 *         scaling matrix.                                        *
 *                                                                *
 * P_data is a pointer to user data - the same as the P_data      *
 *           parameter passed to KINSpgmr.                        *
 *                                                                *
 * vtemp1, vtemp2  two N_Vector temporaries for use by preconset  *
 *                                                                *
 *                                                                *
 * Returned value:                                                *
 * The value to be returned by the PrecSetup function is a flag   *
 * indicating whether it was successful.  This value should be    *
 *   0  if successful,                                            *
 *   1  if failure, in which case KINSol stops                    *
 *                                                                *
 *                                                                *
 ******************************************************************/

typedef int (*KINSpgmrPrecSetupFn)(N_Vector uu, N_Vector uscale,
                                   N_Vector fval, N_Vector fscale,
                                   void *P_data,
                                   N_Vector vtemp1, N_Vector vtemp2);


/******************************************************************
 *                                                                *           
 * Type : KINSpgmrPrecSolveFn                                     *
 *----------------------------------------------------------------*
 * The user-supplied preconditioner solve function PrecSolve      *
 * is to solve a linear system P x = r in which the matrix P is   *
 * the (right) preconditioner matrix P.                           *
 *                                                                *
 * PrecSetup should not modify the contents of the iterate        *
 * array uu  or the current function value array  fval as those   *
 * are used elsewhere in the iteration process.                   *
 *                                                                *
 * A function PrecSolve must have the prototype given below.      *
 * Its parameters are as follows:                                 *
 *                                                                *
 * uu      an N_Vector giving the current iterate for the system. *
 *                                                                *
 * uscale  an N_Vector giving the diagonal entries of the uu-     *
 *         scaling matrix.                                        *
 *                                                                *
 * fval    an N_Vector giving the current function value          *
 *                                                                *
 * fscale  an N_Vector giving the diagonal entries of the func-   *
 *         scaling matrix.                                        *
 *                                                                *
 * vv      an N_Vector to hold the RHS vector r on input and      *
 *         the result vector x on return                          *
 *                                                                *
 * P_data is a pointer to user data - the same as the P_data      *
 *          parameter passed to KINSpgmr.                         *
 *                                                                *
 * vtemp   an N_Vector work space.                                *
 *                                                                *
 * Returned value:                                                *
 * The value to be returned by the PrecSolve function is a flag   *
 * indicating whether it was successful.  This value should be    *
 *   0 if successful,                                             *
 *   1 if failure, in which case KINSol stops                     *
 *                                                                *
 ******************************************************************/
  
typedef int (*KINSpgmrPrecSolveFn)(N_Vector uu, N_Vector uscale, 
                                   N_Vector fval, N_Vector fscale, 
                                   N_Vector vv, void *P_data,
                                   N_Vector vtemp);

/********************************************************************
 *                                                                  *
 * type : KINSpgmrJacTimesVecFn                                     *
 *                                                                  *
 *    The user-supplied J times v routine (optional) where J is the *
 *    Jacobian matrix dF/du, or an approximation to it, and v is a  *
 *    given  vector.  This routine computes the product Jv = J*v.   *
 *    It should return 0 if successful and a nonzero int otherwise. *
 *                                                                  *
 *   v is the N_Vector to be multiplied by J                        *
 *     (preconditioned and unscaled as received)                    *
 *                                                                  *
 *   Jv is the N_Vector resulting from the application of J to v    *
 *                                                                  * 
 *   uu  is the N_Vector equal to the current iterate u             *
 *                                                                  *
 *   new_uu   is an input flag indicating whether or not the uu     *
 *     vector has been changed since the last call to this function.*
 *     If this function computes and saves Jacobian data, then      *
 *     this computation can be skipped if new_uu = FALSE.           *
 *                                                                  *
 *   J_data is a pointer to the structure used to handle data for   *
 *     the evaluation of the jacobian of func                       *
 *                                                                  *
 ********************************************************************/

typedef int (*KINSpgmrJacTimesVecFn)(N_Vector v, N_Vector Jv,
                                     N_Vector uu, booleantype *new_uu, 
                                     void *J_data);
  
 
/******************************************************************
 *                                                                *
 * Function : KINSpgmr                                            *
 *----------------------------------------------------------------*
 * A call to the KINSpgmr function links the main KINSol solver   *
 * with the KINSpgmr linear solver. Among other things, it sets   *
 * the generic names linit, lsetup, lsolve, and lfree to the      *
 * specific names for this package:                               *
 *                   KINSpgmrInit                                 *
 *                          KINSpgmrSetup                         *
 *                                  KINSpgmrSolve                 *
 *                                              KINSpgmrFree      *
 *                                                                *
 * kinmem  is the pointer to KINSol memory returned by            *
 *             KINCreate.                                         *
 *                                                                *
 * maxl      is the maximum Krylov dimension. This is an          *
 *             optional input to the KINSpgmr solver. Pass 0 to   *
 *             use the default value MIN(Neq, KINSPGMR_MAXL=10).  *
 *                                                                *
 * KINSpgmr returns an int equal to one of the following:         *
 *      SUCCESS, KIN_MEM_NULL, KINSPGMR_MEM_FAIL, SPGMR_MEM_FAIL  *
 *                                                                *
 ******************************************************************/

int KINSpgmr(void *kinmem,  int maxl);

/******************************************************************
 * Optional inputs to the KINSPGMR linear solver                  *
 *----------------------------------------------------------------*
 *                                                                *
 * KINSpgmrSetMaxRestarts specifies the maximum number of restarts*
 *           to be used in the GMRES algorithm.  maxrs must be a  *
 *           non-negative integer.  Pass 0 to specify no restarts.*
 *           Default is 0.                                        *
 * KINSpgmrSetPrecSetupFn specifies the PrecSetup function.       *
 *           Default is NULL.                                     *
 * KINSpgmrSetPrecSolveFn specifies the PrecSolve function.       *
 *           Default is NULL.                                     *
 * KINSpgmrSetPrecData specifies a pointer to user preconditioner *
 *           data. This pointer is passed to PrecSetup and        *
 *           PrecSolve every time these routines are called.      *
 *           Default is NULL.                                     *
 * KINSpgmrSetJacTimesVecFn specifies the jtimes function.        *
 *           Default is to use an internal finite difference      *
 *           approximation routine.                               *
 * KINSpgmrSetJAcData specifies a pointer to user Jac times vec   *
 *           data. This pointer is passed to jtimes every time    *
 *           this routine is called.                              *
 *           Default is NULL.                                     *
 *                                                                *
 ******************************************************************/

int KINSpgmrSetMaxRestarts(void *kinmem, int maxrs);
int KINSpgmrSetPrecSetupFn(void *kinmem, KINSpgmrPrecSetupFn pset);
int KINSpgmrSetPrecSolveFn(void *kinmem, KINSpgmrPrecSolveFn psolve);
int KINSpgmrSetPrecData(void *kinmem, void *P_data);
int KINSpgmrSetJacTimesVecFn(void *kinmem, KINSpgmrJacTimesVecFn jtimes);
int KINSpgmrSetJacData(void *kinmem, void *J_data);

/******************************************************************
 * Optional outputs from the KINSPGMR linear solver               *
 *----------------------------------------------------------------*
 *                                                                *
 * KINSpgmrGetIntWorkSpace returns the integer workspace used by  *
 *     KINSPGMR.                                                  *
 * KINSpgmrGetRealWorkSpace returns the real workspace used by    *
 *     KINSPGMR.                                                  *
 * KINSpgmrGetNumPrecEvals returns the number of preconditioner   *
 *     evaluations, i.e. the number of calls made to PrecSetup    *
 *     with jok==FALSE.                                           *
 * KINSpgmrGetNumPrecSolves returns the number of calls made to   *
 *     PrecSolve.                                                 *
 * KINSpgmrGetNumLinIters returns the number of linear iterations.*
 * KINSpgmrGetNumConvFails returns the number of linear           *
 *     convergence failures.                                      *
 * KINSpgmrGetNumJtimesEvals returns the number of calls to jtimes*
 * KINSpgmrGetNumFuncEvals returns the number of calls to the user*
 *     res routine due to finite difference Jacobian times vector *
 *     evaluation.                                                *
 *                                                                *
 ******************************************************************/

int KINSpgmrGetIntWorkSpace(void *kinmem, long int *leniwSG);
int KINSpgmrGetRealWorkSpace(void *kinmem, long int *lenrwSG);
int KINSpgmrGetNumPrecEvals(void *kinmem, int *npevals);
int KINSpgmrGetNumPrecSolves(void *kinmem, int *npsolves);
int KINSpgmrGetNumLinIters(void *kinmem, int *nliters);
int KINSpgmrGetNumConvFails(void *kinmem, int *nlcfails);
int KINSpgmrGetNumJtimesEvals(void *kinmem, int *njvevals);
int KINSpgmrGetNumFuncEvals(void *kinmem, int *nfevalsSG); 

/******************************************************************
 *                                                                *           
 * Types : KINSpgmrMemRec, KINSpgmrMem                            *
 *----------------------------------------------------------------*
 * The type KINSpgmrMem is pointer to a KINSpgmrMemRec. This      *
 * structure contains KINSpgmr solver-specific data.              *
 *                                                                *
 ******************************************************************/

typedef struct {

  int  g_maxl;        /* maxl = maximum dimension of the Krylov space   */     
  int  g_pretype;     /* preconditioning type--for Spgmr call           */
  int  g_gstype;      /* gram schmidt type --  for Spgmr call           */
  booleantype g_new_uu;  /* flag that a new uu has been created--
                            indicating that a call to generate a new
                            user-supplied Jacobian is required */
  int g_maxlrst;      /* max number of linear solver restarts allowed;
                         default is zero  */
  int g_nli;          /* nli = total number of linear iterations        */
  int g_npe;          /* npe = total number of precondset calls         */
  int g_nps;          /* nps = total number of psolve calls             */
  int g_ncfl;         /* ncfl = total number of convergence failures    */

  int g_nfeSG;        /* nfeSG = total number of calls to func          */    
  int g_njtimes;      /* njtimes = total number of calls to jtimes      */

  KINSpgmrPrecSetupFn g_pset; 
                      /* PrecSetup = user-supplied routine to
                         compute a preconditioner                       */

  KINSpgmrPrecSolveFn g_psolve; 
                      /* PrecSolve = user-supplied routine to 
                         solve preconditioner linear system             */ 

  KINSpgmrJacTimesVecFn g_jtimes;
                      /* jtimes = user-supplied routine to optionally
                         compute the product J v as required by Spgmr   */ 

  void *g_P_data;     /* P_data is a memory block passed to psolve and pset */
  void *g_J_data;     /* J_data is a memory block passed to jtimes */

  SpgmrMem g_spgmr_mem;  /* spgmr_mem is memory used by the generic Spgmr solver */

} KINSpgmrMemRec, *KINSpgmrMem;


#endif
#ifdef __cplusplus
}
#endif
