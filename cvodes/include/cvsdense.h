/*******************************************************************
 *                                                                 *
 * File          : cvsdense.h                                      *
 * Programmers   : Scott D. Cohen, Alan C. Hindmarsh, and          *
 *                 Radu Serban @ LLNL                              *
 * Version of    : 11 June 2003                                    *
 *-----------------------------------------------------------------*
 * Copyright (c) 2002, The Regents of the University of California * 
 * Produced at the Lawrence Livermore National Laboratory          *
 * All rights reserved                                             *
 * For details, see sundials/cvodes/LICENSE                        *
 *-----------------------------------------------------------------*
 * This is the header file for the CVODES dense linear solver,     *
 * CVSDENSE.                                                       *
 *                                                                 *
 * Note: The type integertype must be large enough to store the    *
 * value of the linear system size N.                              *
 *                                                                 *
 *******************************************************************/

#ifdef __cplusplus     /* wrapper to enable C++ usage */
extern "C" {
#endif

#ifndef _cvsdense_h
#define _cvsdense_h

#include <stdio.h>
#include "cvodes.h"
#include "sundialstypes.h"
#include "dense.h"
#include "nvector.h"

/******************************************************************
 *                                                                *
 * CVSDENSE solver constants                                      *
 *----------------------------------------------------------------*
 * CVD_MSBJ  : maximum number of steps between dense Jacobian     *
 *             evaluations                                        *
 *                                                                *
 * CVD_DGMAX : maximum change in gamma between dense Jacobian     *
 *             evaluations                                        *
 *                                                                *
 ******************************************************************/

#define CVD_MSBJ  50   

#define CVD_DGMAX RCONST(0.2)  
 
/******************************************************************
 *                                                                *           
 * Type : CVDenseJacFn                                            *
 *----------------------------------------------------------------*
 * A dense Jacobian approximation function Jac must have the      *
 * prototype given below. Its parameters are:                     *
 *                                                                *
 * N is the length of all vector arguments.                       *
 *                                                                *
 * J is the dense matrix (of type DenseMat) that will be loaded   *
 * by a CVDenseJacFn with an approximation to the Jacobian matrix *
 * J = (df_i/dy_j) at the point (t,y).                            *
 * J is preset to zero, so only the nonzero elements need to be   *
 * loaded. Two efficient ways to load J are:                      *
 *                                                                *
 * (1) (with macros - no explicit data structure references)      *
 *     for (j=0; j < n; j++) {                                    *
 *       col_j = DENSE_COL(J,j);                                  *
 *       for (i=0; i < n; i++) {                                  *
 *         generate J_ij = the (i,j)th Jacobian element           *
 *         col_j[i] = J_ij;                                       *
 *       }                                                        *
 *     }                                                          *
 *                                                                *  
 * (2) (without macros - explicit data structure references)      *
 *     for (j=0; j < n; j++) {                                    *
 *       col_j = (J->data)[j];                                    *
 *       for (i=0; i < n; i++) {                                  *
 *         generate J_ij = the (i,j)th Jacobian element           *
 *         col_j[i] = J_ij;                                       *
 *       }                                                        *
 *     }                                                          *
 *                                                                *
 * The DENSE_ELEM(A,i,j) macro is appropriate for use in small    *
 * problems in which efficiency of access is NOT a major concern. *
 *                                                                *
 * t is the current value of the independent variable.            *
 *                                                                *
 * y is the current value of the dependent variable vector,       *
 *      namely the predicted value of y(t).                       *
 *                                                                *
 * fy is the vector f(t,y).                                       *
 *                                                                *
 * jac_data is a pointer to user data - the same as the jac_data  *
 *          parameter passed to CVDense.                          *
 *                                                                *
 * tmp1, tmp2, and tmp3 are pointers to memory allocated for      *
 * vectors of length N which can be used by a CVDenseJacFn        *
 * as temporary storage or work space.                            *
 *                                                                *
 ******************************************************************/
  
typedef void (*CVDenseJacFn)(integertype N, DenseMat J, realtype t, 
                             N_Vector y, N_Vector fy, void *jac_data,
                             N_Vector tmp1, N_Vector tmp2, N_Vector tmp3);
 
 
/******************************************************************
 *                                                                *
 * Function : CVDense                                             *
 *----------------------------------------------------------------*
 * A call to the CVDense function links the main CVODES integrator*
 * with the CVSDENSE linear solver.                               *
 *                                                                *
 * cvode_mem is the pointer to CVODES memory returned by          *
 *              CVodeCreate.                                      *
 *                                                                *
 * N is the size of the ODE system.                               *
 *                                                                *
 * The return values of CVDense are:                              *
 *    SUCCESS   = 0  if successful                                *
 *    LMEM_FAIL = -1 if there was a memory allocation failure     *
 *                                                                *
 ******************************************************************/
  
int CVDense(void *cvode_mem, integertype N); 

/******************************************************************
 * Optional inputs to the CVSDENSE linear solver                  *
 *----------------------------------------------------------------*
 *                                                                *
 * CVDenseSetJacFn specifies the dense Jacobian approximation     *
 *         routine to be used. A user-supplied djac routine must  *
 *         be of type CVDenseJacFn.                               *
 *         By default, a difference quotient routine CVDenseDQJac,*
 *         supplied with this solver is used.                     *
 * CVDenseSetJacData specifies a pointer to user data which is    *
 *         passed to the djac routine every time it is called.    *
 *                                                                *
 ******************************************************************/

int CVDenseSetJacFn(void *cvode_mem, CVDenseJacFn djac);
int CVDenseSetJacData(void *cvode_mem, void *jac_data);

/******************************************************************
 * Optional outputs from the CVSDENSE linear solver               *
 *----------------------------------------------------------------*
 *                                                                *
 * CVDenseGetIntWorkSpace returns the integer workspace used by   *
 *     CVSDENSE.                                                  *
 * CVDenseGetRealWorkSpace returns the real workspace used by     *
 *     CVSDENSE.                                                  *
 * CVDenseGetNumJacEvals returns the number of calls made to the  *
 *     Jacobian evaluation routine djac.                          *
 * CVDenseGetNumRhsEvals returns the number of calls to the user  *
 *     f routine due to finite difference Jacobian evaluation.    *
 *                                                                *
 ******************************************************************/

int CVDenseGetIntWorkSpace(void *cvode_mem, long int *leniwD);
int CVDenseGetRealWorkSpace(void *cvode_mem, long int *lenrwD);
int CVDenseGetNumJacEvals(void *cvode_mem, int *njevalsD);
int CVDenseGetNumRhsEvals(void *cvode_mem, int *nfevalsD);

/******************************************************************
 *                                                                *           
 * Types : CVDenseMemRec, CVDenseMem                              *
 *----------------------------------------------------------------*
 * The type CVDenseMem is pointer to a CVDenseMemRec. This        *
 * structure contains CVDense solver-specific data.               *
 *                                                                *
 ******************************************************************/

typedef struct {

  integertype d_n;    /* problem dimension                      */

  CVDenseJacFn d_jac; /* jac = Jacobian routine to be called    */

  DenseMat d_M;       /* M = I - gamma J, gamma = h / l1        */
  
  integertype *d_pivots;  /* pivots = pivot array for PM = LU   */
  
  DenseMat d_savedJ;  /* savedJ = old Jacobian                  */
  
  int  d_nstlj;       /* nstlj = nst at last Jacobian eval.     */
  
  int d_nje;          /* nje = no. of calls to jac              */

  int d_nfeD;         /* nfeD = no. of calls to f               */
  
  void *d_J_data;     /* J_data is passed to jac                */
  
} CVDenseMemRec, *CVDenseMem;

#endif

#ifdef __cplusplus
}
#endif
