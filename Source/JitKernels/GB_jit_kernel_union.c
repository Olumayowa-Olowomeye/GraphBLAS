//------------------------------------------------------------------------------
// GB_jit_kernel_union.c: C=A+B, C<#M>=A+B, for eWiseUnion
//------------------------------------------------------------------------------

// SuiteSparse:GraphBLAS, Timothy A. Davis, (c) 2017-2023, All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

//------------------------------------------------------------------------------

GB_JIT_KERNEL_UNION_PROTO (GB_jit_kernel) ;
GB_JIT_KERNEL_UNION_PROTO (GB_jit_kernel)
{
    #define GB_IS_EWISEUNION 1
    GB_A_TYPE alpha_scalar = (*((GB_A_TYPE *) alpha_scalar_in)) ;
    GB_B_TYPE beta_scalar  = (*((GB_B_TYPE *) beta_scalar_in )) ;
    #include "GB_add_template.c"
    return (GrB_SUCCESS) ;
}

