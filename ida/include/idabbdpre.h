/*
 * -----------------------------------------------------------------
 * $Revision: 1.15 $
 * $Date: 2004-04-29 22:34:08 $
 * ----------------------------------------------------------------- 
 * Programmers: Allan G. Taylor, Alan C. Hindmarsh, and
 *              Radu Serban @ LLNL
 * -----------------------------------------------------------------
 * Copyright (c) 2002, The Regents of the University of California  
 * Produced at the Lawrence Livermore National Laboratory
 * All rights reserved
 * For details, see sundials/ida/LICENSE
 * -----------------------------------------------------------------
 * This is the header file for the IDABBDPRE module, for a         
 * band-block-diagonal preconditioner, i.e. a block-diagonal       
 * matrix with banded blocks, for use with IDA/IDAS and IDASpgmr.  
 *                                                                 
 * Summary:                                                        
 *                                                                 
 * These routines provide a preconditioner matrix that is          
 * block-diagonal with banded blocks.  The blocking corresponds    
 * to the distribution of the dependent variable vector y among    
 * the processors.  Each preconditioner block is generated from    
 * the Jacobian of the local part (on the current processor) of a  
 * given function G(t,y,y') approximating F(t,y,y').  The blocks   
 * are generated by a difference quotient scheme on each processor 
 * independently.  This scheme utilizes an assumed banded          
 * structure with given half-bandwidths, mudq and mldq.            
 * However, the banded Jacobian block kept by the scheme has       
 * half-bandwiths mukeep and mlkeep, which may be smaller.         
 *                                                                 
 * The user's calling program should have the following form:      
 *                                                                 
 *   #include "idabbdpre.h"                                        
 *   #include "nvector_parallel.h"                                 
 *   ...                                                           
 *   void *p_data;                                                 
 *   ...                                                           
 *   nvSpec = NV_SpecInit_Parallel(...);                           
 *   ...                                                           
 *   ida_mem = IDACreate(...);                                     
 *   ier = IDAMalloc(...);                                         
 *   ...                                                           
 *   p_data = IDABBDPrecAlloc(ida_mem, Nlocal, mudq, mldq,         
 *                      mukeep, mlkeep, dq_rel_yy, glocal, gcomm); 
 *   flag = IDABBDSpgmr(cvode_mem, maxl, p_data);                  
 *   ...                                                           
 *   ier = IDASolve(...);                                          
 *   ...                                                           
 *   IDABBDFree(p_data);                                           
 *   ...                                                           
 *   IDAFree(...);                                                 
 *                                                                 
 *   NV_SpecFree_Parallel(nvSpec);                                 
 *                                                                 
 * The user-supplied routines required are:                        
 *                                                                 
 *   res  is the function F(t,y,y') defining the DAE system to     
 *   be solved:  F(t,y,y') = 0.                                    
 *                                                                 
 *   glocal  is the function defining a local approximation        
 *   G(t,y,y') to F, for the purposes of the preconditioner.       
 *                                                                 
 *   gcomm  is the function performing communication needed        
 *                   for glocal.                                   
 *                                                                 
 *                                                                 
 * Notes:                                                          
 *                                                                 
 * 1) This header file is included by the user for the definition  
 *    of the IBBDPrecData type and for needed function prototypes. 
 *                                                                 
 * 2) The IDABBDPrecAlloc call includes half-bandwidths mudq and   
 *    mldq to be used in the approximate Jacobian.  They need      
 *    not be the true half-bandwidths of the Jacobian of the       
 *    local block of G, when smaller values may provide a greater  
 *    efficiency. Similarly, mukeep and mlkeep, specifying the     
 *    bandwidth kept for the approximate Jacobian, need not be     
 *    the true half-bandwidths. Also, mukeep, mlkeep, mudq, and    
 *    mldq need not be the same on every processor.                
 *                                                                 
 * 3) The actual name of the user's res function is passed to      
 *    IDAMalloc, and the names of the user's glocal and gcomm      
 *    functions are passed to IDABBDPrecAlloc.                     
 *                                                                 
 * 4) The pointer to the user-defined data block res_data, which   
 *    is set through IDASetRdata is also available to the user     
 *    in glocal and gcomm.                                         
 *                                                                 
 * 5) Optional outputs specific to this module are available by    
 *    way of routines listed below. These include work space sizes 
 *    and the cumulative number of glocal calls.  The costs        
 *    associated with this module also include nsetups banded LU   
 *    factorizations, nsetups gcomm calls, and nps banded          
 *    backsolve calls, where nsetups and nps are integrator        
 *    optional outputs.                                            
 * -----------------------------------------------------------------
 */

#ifdef __cplusplus     /* wrapper to enable C++ usage */
extern "C" {
#endif
#ifndef _ibbdpre_h
#define _ibbdpre_h

#include "sundialstypes.h"
#include "nvector.h"
#include "band.h"

/******************************************************************
 * Type : IDALocalFn                                              *
 *----------------------------------------------------------------*        
 * The user must supply a function G(t,y,y') which approximates   *
 * the function F for the system F(t,y,y') = 0, and which is      *
 * computed locally (without inter-processor communication).      *
 * (The case where G is mathematically identical to F is allowed.)*
 * The implementation of this function must have type IDALocalFn. *
 *                                                                *
 * This function takes as input the independent variable value tt,*
 * the current solution vector yy, the current solution           *
 * derivative vector yp, and a pointer to the user-defined data   *
 * block res_data.  It is to compute the local part of G(t,y,y')  *
 * and store it in the vector gval. (Providing memory for yy and  *
 * gval is handled within this preconditioner module.) It is      *
 * expected that this routine will save communicated data in work *
 * space defined by the user, and made available to the           *
 * preconditioner function for the problem. The res_data          *
 * parameter is the same as that passed by the user to the        *
 * IDAMalloc routine.                                             *
 *                                                                *
 * A IDALocalFn glocal is to return an int, defined in the same   *
 * way as for the residual function: 0 (success), +1 or -1 (fail).*
 ******************************************************************/

typedef int (*IDALocalFn)(long int Nlocal, realtype tt, 
                          N_Vector yy, N_Vector yp, N_Vector gval, 
                          void *res_data);
 

/******************************************************************
 * Type : IDACommFn                                               *
 *----------------------------------------------------------------*        
 * The user must supply a function of type IDACommFn which        *
 * performs all inter-processor communication necessary to        *
 * evaluate the approximate system function described above.      *
 *                                                                *
 * This function takes as input the solution vectors yy and yp,   *
 * and a pointer to the user-defined data block res_data. The     *
 * res_data parameter is the same as that passed by the user to   *
 * the IDAMalloc routine.                                         *
 *                                                                *
 * The IDACommFn gcomm is expected to save communicated data in   *
 * space defined with the structure *res_data.                    *
 *                                                                *
 * A IDACommFn gcomm returns an int value equal to 0 (success),   *
 * > 0 (recoverable error), or < 0 (unrecoverable error).         *
 *                                                                *
 * Each call to the IDACommFn is preceded by a call to the system *
 * function res with the same vectors yy and yp. Thus the         *
 * IDACommFn gcomm can omit any communications done by res if     *
 * relevant to the evaluation of the local function glocal.       *
 ******************************************************************/

typedef int (*IDACommFn)(long int Nlocal, realtype tt, 
                         N_Vector yy, N_Vector yp, 
                         void *res_data);


/*********************** Definition of IBBDPrecData *****************/

typedef struct {

  /* passed by user to IDABBDPrecAlloc, used by 
     IDABBDPrecSetup/IDABBDPrecSolve functions: */
  long int mudq, mldq, mukeep, mlkeep;
  realtype rel_yy;
  IDALocalFn glocal;
  IDACommFn gcomm;

 /* allocated for use by IDABBDPrecSetup */
  N_Vector tempv4;

  /* set by IDABBDPrecon and used by IDABBDPrecSolve: */
  BandMat PP;
  long int *pivots;

  /* set by IDABBDPrecAlloc and used by IDABBDPrecSetup */
  long int n_local;

  /* available for optional output: */
  long int rpwsize;
  long int ipwsize;
  long int nge;

  /* Pointer to ida_mem */
  IDAMem IDA_mem;

} *IBBDPrecData;


/******************************************************************
 * Function : IDABBDPrecAlloc                                     *
 *----------------------------------------------------------------*
 * IDABBDPrecAlloc allocates and initializes an IBBDPrecData      *
 * structure to be passed to IDASpgmr (and used by                *
 * IDABBDPrecSetup and IDABBDPrecSol).                            *
 *                                                                *
 * The parameters of IDABBDPrecAlloc are as follows:              *
 *                                                                *
 * ida_mem  is a pointer to the memory blockreturned by IDACreate.*
 *                                                                *
 * Nlocal  is the length of the local block of the vectors yy etc.*
 *         on the current processor.                              *
 *                                                                *
 * mudq, mldq  are the upper and lower half-bandwidths to be used *
 *         in the computation of the local Jacobian blocks.       *
 *                                                                *
 * mukeep, mlkeep are the upper and lower half-bandwidths to be   *
 *         used in saving the Jacobian elements in the local      *
 *         block of the preconditioner matrix PP.                 *
 *                                                                *
 * dq_rel_yy is an optional input.  It is the relative increment  *
 *         to be used in the difference quotient routine for      *
 *         Jacobian calculation in the preconditioner.  The       *
 *         default is sqrt(unit roundoff), and specified by       *
 *         passing dq_rel_yy = 0.                                 *
 *                                                                *
 * glocal    is the name of the user-supplied function G(t,y,y')  *
 *         that approximates F and whose local Jacobian blocks    *
 *         are to form the preconditioner.                        *
 *                                                                *
 * gcomm   is the name of the user-defined function that performs *
 *         necessary inter-processor communication for the        *
 *         execution of glocal.                                   *
 *                                                                *
 * IDABBDPrecAlloc returns the storage allocated (type *void),    *
 * or NULL if the request for storage cannot be satisfied.        *
 ******************************************************************/

void *IDABBDPrecAlloc(void *ida_mem, long int Nlocal, 
		      long int mudq, long int mldq, 
		      long int mukeep, long int mlkeep, 
		      realtype dq_rel_yy, 
		      IDALocalFn glocal, IDACommFn gcomm);

/******************************************************************
 * Function : IDABBDSpgmr                                         *
 *----------------------------------------------------------------*
 * IDABBDSpgmr links the IDABBDPRE preconditioner to the IDASPGMR *
 * linear solver. It performs the following actions:              *
 *  1) Calls the IDASPGMR specification routine and attaches the  *
 *     IDASPGMR linear solver to the IDA solver;                  *
 *  2) Sets the preconditioner data structure for IDASPGMR        *
 *  3) Sets the preconditioner setup routine for IDASPGMR         *
 *  4) Sets the preconditioner solve routine for IDASPGMR         *
 *                                                                *
 * Its first 2 arguments are the same as for IDASpgmr (see        *
 * idaspgmr.h). The last argument is the pointer to the IDABBDPRE *
 * memory block returned by IDABBDPrecAlloc.                      * 
 * Note that the user need not call IDASpgmr anymore.             *
 *                                                                *
 * Possible return values are:                                    *
 *   (from ida.h)    SUCCESS                                      *
 *                   LIN_NO_MEM                                   *
 *                   LMEM_FAIL                                    *
 *                   LIN_NO_LMEM                                  *
 *                   LIN_ILL_INPUT                                *
 *   Additionaly, if IDABBDPrecAlloc was not previously called,   *
 *   IDABBDSpgmr returns BBDP_NO_PDATA (defined below).           *
 *                                                                *
 ******************************************************************/

int IDABBDSpgmr(void *ida_mem, int maxl, void *p_data);

/******************************************************************
 * Function : IDABBDPrecReInit                                    *
 *----------------------------------------------------------------*
 * IDABBDPrecReInit re-initializes the IDABBDPRE module when      *
 * solving a sequence of problems of the same size with           *
 * IDASPGMR/IDABBDPRE provided there is no change in Nlocal,      *
 * mukeep, or mlkeep.  After solving one problem, and after       *
 * calling IDAReInit to re-initialize the integrator for a        *
 * subsequent problem, call IDABBDPrecReInit.                     *
 * Then call IDAReInitSpgmr or IDASpgmr, if necessary, to         *
 * re-initialize the Spgmr linear solver, depending on changes    *
 * made in its input parameters, before calling IDASolve.         *
 *                                                                *
 * The first argument to IDABBDPrecReInit must be the pointer     *
 * p_data that was returned by IDABBDPrecAlloc.  All other        *
 * arguments have the same names and meanings as those of         *
 * IDABBDPrecAlloc.                                               *
 *                                                                *
 * The return value of IDABBDPrecReInit is 0, indicating success. *
 ******************************************************************/

int IDABBDPrecReInit(void *p_data, 
		     long int mudq, long int mldq,
		     realtype dq_rel_yy, 
		     IDALocalFn glocal, IDACommFn gcomm); 

/******************************************************************
 * Function : IDABBDPrecFree                                      *
 *----------------------------------------------------------------*
 * IDABBDPrecFree frees the memory block p_data allocated by the  *
 * call to IDABBDPrecAlloc.                                       *
 ******************************************************************/

void IDABBDPrecFree(void *p_data);

/******************************************************************
 * Optional outputs for IDABBDPRE                                 *
 *----------------------------------------------------------------*
 *                                                                *
 * IDABBDPrecGetIntWorkSpace returns the integer workspace for    *
 *    IBBDPRE.                                                    *
 * IDABBDPrecGetRealWorkSpace returns the real workspace for      *
 *    IBBDPRE.                                                    *
 * IDABBDPrecGetNumGfnEvals returns the number of calls to the    *
 *    uer glocal function.                                        *
 *                                                                *
 ******************************************************************/

int IDABBDPrecGetIntWorkSpace(void *p_data, long int *leniwBBDP);
int IDABBDPrecGetRealWorkSpace(void *p_data, long int *lenrwBBDP);
int IDABBDPrecGetNumGfnEvals(void *p_data, long int *ngevalsBBDP);

/* Return values for IDABBDPrecGet* functions */
/* OKAY = 0 */
enum { BBDP_NO_PDATA = -11 };

/* Prototypes of IDABBDPrecSetup and IDABBDPrecSolve */

int IDABBDPrecSetup(realtype tt, 
		    N_Vector yy, N_Vector yp, N_Vector rr, 
		    realtype cj, void *p_data,
		    N_Vector tempv1, N_Vector tempv2, N_Vector tempv3);
 
int IDABBDPrecSolve(realtype tt, 
		    N_Vector yy, N_Vector yp, N_Vector rr, 
		    N_Vector rvec, N_Vector zvec,
		    realtype cj, realtype delta,
		    void *p_data, N_Vector tempv);

#endif
#ifdef __cplusplus
}
#endif
