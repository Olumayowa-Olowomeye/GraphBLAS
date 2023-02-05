
//------------------------------------------------------------------------------
// GB_unop:  hard-coded functions for each built-in unary operator
//------------------------------------------------------------------------------

// SuiteSparse:GraphBLAS, Timothy A. Davis, (c) 2017-2022, All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

//------------------------------------------------------------------------------

// If this file is in the Generated2/ folder, do not edit it
// (it is auto-generated from Generator/*).

#include "GB.h"
#ifndef GBCUDA_DEV
#include "GB_control.h"
#include "GB_unop__include.h"

// C=unop(A) is defined by the following types and operators:

// op(A)  function:  GB (_unop_apply__acosh_fc64_fc64)
// op(A') function:  GB (_unop_tran__acosh_fc64_fc64)

// C type:   GxB_FC64_t
// A type:   GxB_FC64_t
// cast:     GxB_FC64_t cij = aij
// unaryop:  cij = cacosh (aij)

#define GB_A_TYPE \
    GxB_FC64_t

#define GB_C_TYPE \
    GxB_FC64_t

// declare aij as atype
#define GB_DECLAREA(aij) \
    GxB_FC64_t aij

// aij = Ax [pA]
#define GB_GETA(aij,Ax,pA,A_iso) \
    aij = Ax [pA]

#define GB_CX(p) Cx [p]

// unary operator
#define GB_OP(z, x) \
    z = cacosh (x) ;

// casting
#define GB_CAST(z, aij) \
    GxB_FC64_t z = aij ;

// cij = op (aij)
#define GB_CAST_OP(pC,pA)           \
{                                   \
    /* aij = Ax [pA] */             \
    GxB_FC64_t aij ;              \
    aij = Ax [pA] ;   \
    /* Cx [pC] = op (cast (aij)) */ \
    GxB_FC64_t z = aij ;               \
    Cx [pC] = cacosh (z) ;        \
}

// disable this operator and use the generic case if these conditions hold
#define GB_DISABLE \
    (GxB_NO_ACOSH || GxB_NO_FC64)

//------------------------------------------------------------------------------
// Cx = op (cast (Ax)): apply a unary operator
//------------------------------------------------------------------------------


GrB_Info GB (_unop_apply__acosh_fc64_fc64)
(
    GxB_FC64_t *Cx,               // Cx and Ax may be aliased
    const GxB_FC64_t *Ax,         // A is always non-iso for this kernel
    const int8_t *restrict Ab,  // A->b if A is bitmap
    int64_t anz,
    int nthreads
)
{
    #if GB_DISABLE
    return (GrB_NO_VALUE) ;
    #else
    int64_t p ;
    if (Ab == NULL)
    { 
        #pragma omp parallel for num_threads(nthreads) schedule(static)
        for (p = 0 ; p < anz ; p++)
        {
            GxB_FC64_t aij ;
            aij = Ax [p] ;
            GxB_FC64_t z = aij ;
            Cx [p] = cacosh (z) ;
        }
    }
    else
    { 
        // bitmap case, no transpose; A->b already memcpy'd into C->b
        #pragma omp parallel for num_threads(nthreads) schedule(static)
        for (p = 0 ; p < anz ; p++)
        {
            if (!Ab [p]) continue ;
            GxB_FC64_t aij ;
            aij = Ax [p] ;
            GxB_FC64_t z = aij ;
            Cx [p] = cacosh (z) ;
        }
    }
    return (GrB_SUCCESS) ;
    #endif
}


//------------------------------------------------------------------------------
// C = op (cast (A')): transpose, typecast, and apply a unary operator
//------------------------------------------------------------------------------

GrB_Info GB (_unop_tran__acosh_fc64_fc64)
(
    GrB_Matrix C,
    const GrB_Matrix A,
    int64_t *restrict *Workspaces,
    const int64_t *restrict A_slice,
    int nworkspaces,
    int nthreads
)
{ 
    #if GB_DISABLE
    return (GrB_NO_VALUE) ;
    #else
    #include "GB_unop_transpose.c"
    return (GrB_SUCCESS) ;
    #endif
}

#endif

