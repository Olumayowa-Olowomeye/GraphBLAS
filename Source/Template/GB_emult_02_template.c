//------------------------------------------------------------------------------
// GB_emult_02_template: C = A.*B when A is sparse/hyper and B is bitmap/full
//------------------------------------------------------------------------------

// SuiteSparse:GraphBLAS, Timothy A. Davis, (c) 2017-2023, All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

//------------------------------------------------------------------------------

// C is sparse, with the same sparsity structure as A.  No mask is present, or
// M is bitmap/full.  A is sparse/hyper, and B is bitmap/full.

{

    //--------------------------------------------------------------------------
    // get A, B, and C
    //--------------------------------------------------------------------------

    const int64_t *restrict Ap = A->p ;
    const int64_t *restrict Ah = A->h ;
    const int64_t *restrict Ai = A->i ;
    const int64_t vlen = A->vlen ;

    const int8_t  *restrict Bb = B->b ;

    const int64_t *restrict kfirst_Aslice = A_ek_slicing ;
    const int64_t *restrict klast_Aslice  = A_ek_slicing + A_ntasks ;
    const int64_t *restrict pstart_Aslice = A_ek_slicing + A_ntasks * 2 ;

    const bool A_iso = A->iso ;
    const bool B_iso = B->iso ;

    #ifdef GB_ISO_EMULT
    ASSERT (C->iso) ;
    #else
    ASSERT (!C->iso) ;
    ASSERT (!(A_iso && B_iso)) ;    // one of A or B can be iso, but not both
    const GB_A_TYPE *restrict Ax = (GB_A_TYPE *) A->x ;
    const GB_B_TYPE *restrict Bx = (GB_B_TYPE *) B->x ;
          GB_C_TYPE *restrict Cx = (GB_C_TYPE *) C->x ;
    #endif

    const int64_t  *restrict Cp = C->p ;
          int64_t  *restrict Ci = C->i ;

    //--------------------------------------------------------------------------
    // C=A.*B or C<#M>=A.*B
    //--------------------------------------------------------------------------

    if (M == NULL)
    {

        //----------------------------------------------------------------------
        // C = A.*B
        //----------------------------------------------------------------------

        if (GB_IS_BITMAP (B))
        {

            //------------------------------------------------------------------
            // Method2(a): C=A.*B where A is sparse/hyper and B is bitmap
            //------------------------------------------------------------------

            int tid ;
            #pragma omp parallel for num_threads(A_nthreads) schedule(dynamic,1)
            for (tid = 0 ; tid < A_ntasks ; tid++)
            {
                int64_t kfirst = kfirst_Aslice [tid] ;
                int64_t klast  = klast_Aslice  [tid] ;
                for (int64_t k = kfirst ; k <= klast ; k++)
                {
                    int64_t j = GBH_A (Ah, k) ;
                    int64_t pB_start = j * vlen ;
                    int64_t pA, pA_end, pC ;
                    GB_get_pA_and_pC (&pA, &pA_end, &pC, tid, k, kfirst, klast,
                        pstart_Aslice, Cp_kfirst, Cp, vlen, Ap, vlen) ;
                    for ( ; pA < pA_end ; pA++)
                    { 
                        int64_t i = Ai [pA] ;
                        int64_t pB = pB_start + i ;
                        if (!Bb [pB]) continue ;
                        // C (i,j) = A (i,j) .* B (i,j)
                        Ci [pC] = i ;
                        #ifndef GB_ISO_EMULT
                        GB_DECLAREA (aij) ;
                        GB_GETA (aij, Ax, pA, A_iso) ;     
                        GB_DECLAREB (bij) ;
                        GB_GETB (bij, Bx, pB, B_iso) ;
                        GB_BINOP (GB_CX (pC), aij, bij, i, j) ;
                        #endif
                        pC++ ;
                    }
                }
            }

        }
        else
        {

            //------------------------------------------------------------------
            // Method2(b): C=A.*B where A is sparse/hyper and B is full
            //------------------------------------------------------------------

            int tid ;
            #pragma omp parallel for num_threads(A_nthreads) schedule(dynamic,1)
            for (tid = 0 ; tid < A_ntasks ; tid++)
            {
                int64_t kfirst = kfirst_Aslice [tid] ;
                int64_t klast  = klast_Aslice  [tid] ;
                for (int64_t k = kfirst ; k <= klast ; k++)
                {
                    int64_t j = GBH_A (Ah, k) ;
                    int64_t pB_start = j * vlen ;
                    int64_t pA, pA_end ;
                    GB_get_pA (&pA, &pA_end, tid, k, kfirst, klast,
                        pstart_Aslice, Ap, vlen) ;
                    for ( ; pA < pA_end ; pA++)
                    { 
                        // C (i,j) = A (i,j) .* B (i,j)
                        int64_t i = Ai [pA] ;
                        int64_t pB = pB_start + i ;
                        // Ci [pA] = i ; already defined
                        #ifndef GB_ISO_EMULT
                        GB_DECLAREA (aij) ;
                        GB_GETA (aij, Ax, pA, A_iso) ;
                        GB_DECLAREB (bij) ;
                        GB_GETB (bij, Bx, pB, B_iso) ;
                        GB_BINOP (GB_CX (pA), aij, bij, i, j) ;
                        #endif
                    }
                }
            }
        }

    }
    else
    {

        //----------------------------------------------------------------------
        // Method2(c): C<#M>=A.*B, A is sparse/hyper, M and B are bitmap/full
        //----------------------------------------------------------------------

        const int8_t  *restrict Mb = M->b ;
        const GB_M_TYPE *restrict Mx = (Mask_struct) ? NULL : ((GB_M_TYPE *) M->x) ;
        const size_t msize = M->type->size ;

        int tid ;
        #pragma omp parallel for num_threads(A_nthreads) schedule(dynamic,1)
        for (tid = 0 ; tid < A_ntasks ; tid++)
        {
            int64_t kfirst = kfirst_Aslice [tid] ;
            int64_t klast  = klast_Aslice  [tid] ;
            for (int64_t k = kfirst ; k <= klast ; k++)
            {
                int64_t j = GBH_A (Ah, k) ;
                int64_t pB_start = j * vlen ;
                int64_t pA, pA_end, pC ;
                GB_get_pA_and_pC (&pA, &pA_end, &pC, tid, k, kfirst, klast,
                    pstart_Aslice, Cp_kfirst, Cp, vlen, Ap, vlen) ;
                for ( ; pA < pA_end ; pA++)
                { 
                    int64_t i = Ai [pA] ;
                    int64_t pB = pB_start + i ;
                    if (!GBB_B (Bb, pB)) continue ;
                    bool mij = GBB_M (Mb, pB) && GB_MCAST (Mx, pB, msize) ;
                    mij = mij ^ Mask_comp ;
                    if (!mij) continue ;
                    // C (i,j) = A (i,j) .* B (i,j)
                    Ci [pC] = i ;
                    #ifndef GB_ISO_EMULT
                    GB_DECLAREA (aij) ;
                    GB_GETA (aij, Ax, pA, A_iso) ;     
                    GB_DECLAREB (bij) ;
                    GB_GETB (bij, Bx, pB, B_iso) ;
                    GB_BINOP (GB_CX (pC), aij, bij, i, j) ;
                    #endif
                    pC++ ;
                }
            }
        }
    }
}

