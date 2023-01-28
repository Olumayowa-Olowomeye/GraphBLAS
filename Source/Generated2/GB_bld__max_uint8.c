

//------------------------------------------------------------------------------
// GB_bld:  hard-coded functions for builder methods
//------------------------------------------------------------------------------

// SuiteSparse:GraphBLAS, Timothy A. Davis, (c) 2017-2023, All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

//------------------------------------------------------------------------------

// If this file is in the Generated2/ folder, do not edit it
// (it is auto-generated from Generator/*).

#include "GB.h"
#ifndef GBCUDA_DEV
#include "GB_control.h" 
#include "GB_bld__include.h"

// The reduction is defined by the following types and operators:

// Assemble tuples:    GB (_bld__max_uint8)

// S type:   uint8_t
// T type:   uint8_t
// X type:   uint8_t
// Y type:   uint8_t
// Z type:   uint8_t

// dup:      if (aij > s) { s = aij ; }

#define GB_S_TYPENAME \
    uint8_t

#define GB_T_TYPENAME \
    uint8_t

#define GB_X_TYPENAME \
    uint8_t

#define GB_Y_TYPENAME \
    uint8_t

#define GB_Z_TYPENAME \
    uint8_t

// Array to array

    // W [k] = (ztype) S [i], with typecast
    #define GB_CAST_ARRAY_TO_ARRAY(W,k,S,i)         \
        W [k] = S [i]

    // W [k] += (ztype) S [i], with typecast
    #define GB_ADD_CAST_ARRAY_TO_ARRAY(W,k,S,i)     \
        if (S [i] > W [k]) { W [k] = S [i] ; }

// disable this operator and use the generic case if these conditions hold
#define GB_DISABLE \
    (GxB_NO_MAX || GxB_NO_UINT8 || GxB_NO_MAX_UINT8)

//------------------------------------------------------------------------------
// build a non-iso matrix
//------------------------------------------------------------------------------

GrB_Info GB (_bld__max_uint8)
(
    uint8_t *restrict Tx,
    int64_t  *restrict Ti,
    const uint8_t *restrict Sx,
    int64_t nvals,
    int64_t ndupl,
    const int64_t *restrict I_work,
    const int64_t *restrict K_work,
    const int64_t *restrict tstart_slice,
    const int64_t *restrict tnz_slice,
    int nthreads
)
{ 
    #if GB_DISABLE
    return (GrB_NO_VALUE) ;
    #else
    #include "GB_bld_template.c"
    return (GrB_SUCCESS) ;
    #endif
}

#endif

