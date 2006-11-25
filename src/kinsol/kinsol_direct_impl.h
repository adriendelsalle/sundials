/*
 * -----------------------------------------------------------------
 * $Revision: 1.1 $
 * $Date: 2006-11-22 00:12:50 $
 * ----------------------------------------------------------------- 
 * Programmer: Radu Serban @ LLNL
 * -----------------------------------------------------------------
 * Copyright (c) 2006, The Regents of the University of California.
 * Produced at the Lawrence Livermore National Laboratory.
 * All rights reserved.
 * For details, see the LICENSE file.
 * -----------------------------------------------------------------
 * Common implementation header file for the KINDIRECT linear solvers.
 * -----------------------------------------------------------------
 */

#ifndef _KINDIRECT_IMPL_H
#define _KINDIRECT_IMPL_H

#ifdef __cplusplus  /* wrapper to enable C++ usage */
extern "C" {
#endif

#include <kinsol/kinsol_direct.h>


  /*
   * -----------------------------------------------------------------
   * Types: KINDlsMemRec, KINDlsMem                             
   * -----------------------------------------------------------------
   * The type KINDlsMem is pointer to a KINDlsMemRec.
   * This structure contains KINDIRECT solver-specific data. 
   * -----------------------------------------------------------------
   */

  typedef struct {

    int d_type;              /* SUNDIALS_DENSE or SUNDIALS_BAND              */

    int d_n;                 /* problem dimension                            */

    int d_ml;                /* lower bandwidth of Jacobian                  */
    int d_mu;                /* upper bandwidth of Jacobian                  */ 
    int d_smu;               /* upper bandwith of M = MIN(N-1,d_mu+d_ml)     */

    KINDlsDenseJacFn d_djac; /* dense Jacobian routine to be called          */
    KINDlsBandJacFn d_bjac;  /* band Jacobian routine to be called           */
    void *d_J_data;          /* J_data is passed to jac                      */
    
    DlsMat d_J;              /* problem Jacobian                             */
    
    int *d_pivots;           /* pivot array for PM = LU                      */
    
    long int d_nje;          /* no. of calls to jac                          */
    
    long int d_nfeDQ;        /* no. of calls to F due to DQ Jacobian approx. */
    
    int d_last_flag;         /* last error return flag                       */
    
  } KINDlsMemRec, *KINDlsMem;


  /*
   * -----------------------------------------------------------------
   * Prototypes of internal functions
   * -----------------------------------------------------------------
   */

  int kinDlsDenseDQJac(int N,
                       N_Vector u, N_Vector fu,
                       DlsMat Jac, void *jac_data,
                       N_Vector tmp1, N_Vector tmp2);

  int kinDlsBandDQJac(int N, int mupper, int mlower,
                      N_Vector u, N_Vector fu,
                      DlsMat Jac, void *jac_data,
                      N_Vector tmp1, N_Vector tmp2);

  /*
   * -----------------------------------------------------------------
   * Error Messages
   * -----------------------------------------------------------------
   */

#define MSGD_KINMEM_NULL "KINSOL memory is NULL."
#define MSGD_BAD_NVECTOR "A required vector operation is not implemented."
#define MSGD_MEM_FAIL    "A memory request failed."
#define MSGD_LMEM_NULL   "Linear solver memory is NULL."
#define MSGD_BAD_SIZES   "Illegal bandwidth parameter(s). Must have 0 <=  ml, mu <= N-1."
#define MSGD_JACFUNC_FAILED "The Jacobian routine failed in an unrecoverable manner."

#ifdef __cplusplus
}
#endif

#endif