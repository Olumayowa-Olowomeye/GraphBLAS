//------------------------------------------------------------------------------
// GB_callback.h: typedefs for kernel callbacks
//------------------------------------------------------------------------------

// SuiteSparse:GraphBLAS, Timothy A. Davis, (c) 2017-2023, All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

//------------------------------------------------------------------------------

#ifndef GB_CALLBACK_H
#define GB_CALLBACK_H

#include "GB_callback_proto.h"
#include "GB_AxB_saxpy3.h"
#include "GB_Global.h"

//------------------------------------------------------------------------------
// function pointers to callback methods
//------------------------------------------------------------------------------

typedef GB_CALLBACK_SAXPY3_CUMSUM_PROTO ((*GB_AxB_saxpy3_cumsum_f)) ;
typedef GB_CALLBACK_BITMAP_M_SCATTER_PROTO ((*GB_bitmap_M_scatter_f)) ;
typedef GB_CALLBACK_BITMAP_M_SCATTER_WHOLE_PROTO ((*GB_bitmap_M_scatter_whole_f)) ;
typedef GB_CALLBACK_BIX_ALLOC_PROTO ((*GB_bix_alloc_f)) ;
typedef GB_CALLBACK_EK_SLICE_PROTO ((*GB_ek_slice_f)) ;
typedef GB_CALLBACK_EK_SLICE_MERGE1_PROTO ((*GB_ek_slice_merge1_f)) ;
typedef GB_CALLBACK_FREE_MEMORY_PROTO ((*GB_free_memory_f)) ;
typedef GB_CALLBACK_MALLOC_MEMORY_PROTO ((*GB_malloc_memory_f)) ;
typedef GB_CALLBACK_MEMSET_PROTO ((*GB_memset_f)) ;
typedef GB_CALLBACK_QSORT_1_PROTO ((*GB_qsort_1_f)) ;
typedef GB_CALLBACK_WERK_POP_PROTO ((*GB_werk_pop_f)) ;
typedef GB_CALLBACK_WERK_PUSH_PROTO ((*GB_werk_push_f)) ;

// for debugging only:
typedef GB_CALLBACK_GLOBAL_ABORT_PROTO ((*GB_Global_abort_f)) ;
typedef GB_CALLBACK_FLUSH_GET_PROTO ((*GB_Global_flush_get_f)) ;
typedef GB_CALLBACK_PRINTF_GET_PROTO ((*GB_Global_printf_get_f)) ;

//------------------------------------------------------------------------------
// GB_callback: a struct to pass to kernels to give them their callback methods
//------------------------------------------------------------------------------

typedef struct
{
    GB_AxB_saxpy3_cumsum_f      GB_AxB_saxpy3_cumsum_func ;
    GB_bitmap_M_scatter_f       GB_bitmap_M_scatter_func ;
    GB_bitmap_M_scatter_whole_f GB_bitmap_M_scatter_whole_func ;
    GB_bix_alloc_f              GB_bix_alloc_func ;
    GB_ek_slice_f               GB_ek_slice_func ;
    GB_ek_slice_merge1_f        GB_ek_slice_merge1_func ;
    GB_free_memory_f            GB_free_memory_func ;
    GB_malloc_memory_f          GB_malloc_memory_func ;
    GB_memset_f                 GB_memset_func ;
    GB_qsort_1_f                GB_qsort_1_func ;
    GB_werk_pop_f               GB_werk_pop_func ;
    GB_werk_push_f              GB_werk_push_func ;

    // for debugging only:
    GB_Global_abort_f           GB_Global_abort_func ;
    GB_Global_flush_get_f       GB_Global_flush_get_func ;
    GB_Global_printf_get_f      GB_Global_printf_get_func ;
}
GB_callback_struct ;

GB_GLOBAL GB_callback_struct GB_callback ;

#if defined ( GB_DEBUG ) && defined ( GB_JIT_RUNTIME )
#define GB_GET_DEBUG_FUNCTIONS                                          \
    GB_Global_abort_f                                                   \
        GB_Global_abort = my_callback->GB_Global_abort_func ;           \
    GB_Global_flush_get_f                                               \
        GB_Global_flush_get = my_callback->GB_Global_flush_get_func ;   \
    GB_Global_printf_get_f                                              \
        GB_Global_printf_get = my_callback->GB_Global_printf_get_func ;
#else
#define GB_GET_DEBUG_FUNCTIONS
#endif

#endif

