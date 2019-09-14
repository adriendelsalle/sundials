/* ----------------------------------------------------------------------------
 * Programmer(s): Cody J. Balos @ LLNL
 * ----------------------------------------------------------------------------
 * Based on work by Donald Wilcox @ LBNL
 * ----------------------------------------------------------------------------
 * SUNDIALS Copyright Start
 * Copyright (c) 2002-2019, Lawrence Livermore National Security
 * and Southern Methodist University.
 * All rights reserved.
 *
 * See the top-level LICENSE and NOTICE files for details.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * SUNDIALS Copyright End
 * ----------------------------------------------------------------------------
 * Header file for cuSolverSp batched QR SUNLinearSolver interface.
 * ----------------------------------------------------------------------------*/

#ifndef _SUNLINSOL_SLUDIST_H
#define _SUNLINSOL_SLUDIST_H

#include <cuda_runtime.h>
#include <cusolverSp.h>

#include <sundials/sundials_linearsolver.h>
#include <sundials/sundials_matrix.h>
#include <sundials/sundials_nvector.h>
#include <sunmatrix/sunmatrix_sparse.h>

#ifdef __cplusplus
extern "C" {
#endif


/*
 * ----------------------------------------------------------------------------
 * PART I: cuSolverSp implementation of SUNLinearSolver
 * ----------------------------------------------------------------------------
 */

struct _SUNLinearSolverContent_cuSolverSp_batchQR {
  int                nsubsys;              /* number of subsystems                                 */
  int                subsys_size;          /* size of each subsystem                               */
  int                subsys_nnz;           /* number of nonzeros per subsystem                     */
  int                last_flag;            /* last return flag                                     */
  booleantype        first_factorize;      /* is this the first factorization?                     */
  size_t             internal_size;        /* size of cusolver internal buffer for Q and R         */
  size_t             workspace_size;       /* size of cusolver memory block for num. factorization */
  cusolverSpHandle_t cusolver_handle;      /* cuSolverSp context                                   */
  cusparseMatDescr_t system_description;   /* matrix description                                   */
  realtype*          d_values;             /* device array of matrix A values                      */
  int*               d_rowptr;             /* device array of rowptrs for a subsystem              */
  int*               d_colind;             /* device array of column indices for a subsystem       */
  csrqrInfo_t        info;                 /* opaque cusolver data structure                       */
  void*              workspace;            /* memory block used by cusolver                        */
  const char*        desc;                 /* description of this linear solver                    */
};

typedef struct _SUNLinearSolverContent_cuSolverSp_batchQR *SUNLinearSolverContent_cuSolverSp_batchQR;


/*
 * ----------------------------------------------------------------------------
 * PART II: Functions exported by sunlinsol_sludist
 * ----------------------------------------------------------------------------
 */

SUNDIALS_EXPORT SUNLinearSolver SUNLinSol_cuSolverSp_batchQR(N_Vector y,
                                                             SUNMatrix A,
                                                             int nsubsys,
                                                             int subsys_size,
                                                             int subsys_nnz);


/*
 * ----------------------------------------------------------------------------
 *  cuSolverSp implementations of SUNLinearSolver operations
 * ----------------------------------------------------------------------------
 */

SUNDIALS_EXPORT SUNLinearSolver_Type SUNLinSolGetType_cuSolverSp_batchQR(SUNLinearSolver S);

SUNDIALS_EXPORT SUNLinearSolver_ID SUNLinSolGetID_cuSolverSp_batchQR(SUNLinearSolver S);

SUNDIALS_EXPORT int SUNLinSolInitialize_cuSolverSp_batchQR(SUNLinearSolver S);

SUNDIALS_EXPORT int SUNLinSolSetup_cuSolverSp_batchQR(SUNLinearSolver S,
                                                      SUNMatrix A);

SUNDIALS_EXPORT int SUNLinSolSolve_cuSolverSp_batchQR(SUNLinearSolver S,
                                                      SUNMatrix A,
                                                      N_Vector x,
                                                      N_Vector b,
                                                      realtype tol);

SUNDIALS_EXPORT long int SUNLinSolLastFlag_cuSolverSp_batchQR(SUNLinearSolver S);

SUNDIALS_EXPORT int SUNLinSolSpace_cuSolverSp_batchQR(SUNLinearSolver S,
                                                      long int* lenrwLS,
                                                      long int* leniwLS);

SUNDIALS_EXPORT int SUNLinSolFree_cuSolverSp_batchQR(SUNLinearSolver S);


/*
 * ----------------------------------------------------------------------------
 *  Additional get and set functions.
 * ----------------------------------------------------------------------------
 */

SUNDIALS_EXPORT void SUNLinSol_cuSolverSp_batchQR_GetDescription(SUNLinearSolver S,
                                                                 char** desc); 

SUNDIALS_EXPORT void SUNLinSol_cuSolverSp_batchQR_SetDescription(SUNLinearSolver S,
                                                                 const char* desc);


#ifdef __cplusplus
}
#endif

#endif
