//------------------------------------------------------------------------------
// GB_subassign_method14b: C(I,J)<!M> += A ; using S
//------------------------------------------------------------------------------

// SuiteSparse:GraphBLAS, Timothy A. Davis, (c) 2017-2019, All Rights Reserved.
// http://suitesparse.com   See GraphBLAS/Doc/License.txt for license.

//------------------------------------------------------------------------------

// Method 14b: C(I,J)<!M> += A ; using S

// M:           present
// Mask_comp:   true
// C_replace:   false
// accum:       present
// A:           matrix
// S:           constructed (see also Method 6a)

// Compare with Method 6a, which computes the same thing without creating S.

#define GB_FREE_WORK GB_FREE_2_SLICE

#include "GB_subassign.h"

GrB_Info GB_subassign_method14b
(
    GrB_Matrix C,
    // input:
    const GrB_Index *I,
    const int64_t nI,
    const int Ikind,
    const int64_t Icolon [3],
    const GrB_Index *J,
    const int64_t nJ,
    const int Jkind,
    const int64_t Jcolon [3],
    const GrB_Matrix M,
    const GrB_BinaryOp accum,
    const GrB_Matrix A,
    const GrB_Matrix S,
    GB_Context Context
)
{

    //--------------------------------------------------------------------------
    // get inputs
    //--------------------------------------------------------------------------

    GB_GET_C ;
    GB_GET_MASK ;
    GB_GET_A ;
    GB_GET_S ;
    GB_GET_ACCUM ;

    //--------------------------------------------------------------------------
    // Method 14b: C(I,J)<!M> += A ; using S
    //--------------------------------------------------------------------------

    // Time: Close to optimal.  All entries in A+S must be traversed.

    // Compare with Method 10 and Method 14d

    // Method 14b and 14d are very similar (but Method 14d is suboptimal)

    //--------------------------------------------------------------------------
    // Parallel: Z=A+S (Methods 9, 10, 11c, 12c, 13[abcd], 14[abcd])
    //--------------------------------------------------------------------------

    GB_SUBASSIGN_2_SLICE (A, S) ;

    #pragma omp parallel for num_threads(nthreads) schedule(dynamic,1) \
        reduction(+:nzombies) reduction(&&:ok)
    for (int taskid = 0 ; taskid < ntasks ; taskid++)
    {

        //----------------------------------------------------------------------
        // get the task descriptor
        //----------------------------------------------------------------------

        GB_GET_TASK_DESCRIPTOR ;

        //----------------------------------------------------------------------
        // compute all vectors in this task
        //----------------------------------------------------------------------

        for (int64_t k = kfirst ; task_ok && k <= klast ; k++)
        {

            //------------------------------------------------------------------
            // get A(:,j) and S(:,j)
            //------------------------------------------------------------------

            int64_t j = (Zh == NULL) ? k : Zh [k] ;
            GB_GET_MAPPED_VECTOR (pA, pA_end, pA, pA_end, Ap, j, k, Z_to_X) ;
            GB_GET_MAPPED_VECTOR (pS, pS_end, pB, pB_end, Sp, j, k, Z_to_S) ;

            //------------------------------------------------------------------
            // get M(:,j)
            //------------------------------------------------------------------

            int64_t pM_start, pM_end ;
            GB_VECTOR_LOOKUP (pM_start, pM_end, M, j) ;

            //------------------------------------------------------------------
            // do a 2-way merge of S(:,j) and A(:,j)
            //------------------------------------------------------------------

            // jC = J [j] ; or J is a colon expression
            int64_t jC = GB_ijlist (J, j, Jkind, Jcolon) ;

            // while both list S (:,j) and A (:,j) have entries
            while (pS < pS_end && pA < pA_end)
            {
                int64_t iS = Si [pS] ;
                int64_t iA = Ai [pA] ;

                if (iS < iA)
                { 
                    // S (i,j) is present but A (i,j) is not
                    // ----[C . 1] or [X . 1]-------------------------------
                    // [C . 1]: action: ( C ): no change, with accum
                    // [X . 1]: action: ( X ): still a zombie
                    // ----[C . 0] or [X . 0]-------------------------------
                    // [C . 0]: action: ( C ): no change, with accum
                    // [X . 0]: action: ( X ): still a zombie
                    GB_NEXT (S) ;
                }
                else if (iA < iS)
                { 
                    // S (i,j) is not present, A (i,j) is present
                    GB_GET_MIJ_COMPLEMENT (iA) ;
                    if (mij)
                    { 
                        // ----[. A 1]------------------------------------------
                        // [. A 1]: action: ( insert )
                        // iC = I [iA] ; or I is a colon expression
                        int64_t iC = GB_ijlist (I, iA, Ikind, Icolon) ;
                        GB_D_A_1_matrix ;
                    }
                    GB_NEXT (A) ;
                }
                else
                { 
                    // both S (i,j) and A (i,j) present
                    GB_GET_MIJ_COMPLEMENT (iA) ;
                    if (mij)
                    { 
                        // ----[C A 1] or [X A 1]-------------------------------
                        // [C A 1]: action: ( =A ): A to C no accum
                        // [C A 1]: action: ( =C+A ): apply accum
                        // [X A 1]: action: ( undelete ): zombie lives
                        GB_C_S_LOOKUP ;
                        GB_withaccum_C_A_1_matrix ;
                    }
                    GB_NEXT (S) ;
                    GB_NEXT (A) ;
                }
            }

            if (!task_ok) break ;

            // ignore the remainder of S(:,j)

            // while list A (:,j) has entries.  List S (:,j) exhausted
            while (pA < pA_end)
            { 
                // S (i,j) is not present, A (i,j) is present
                int64_t iA = Ai [pA] ;
                GB_GET_MIJ_COMPLEMENT (iA) ;
                if (mij)
                { 
                    // ----[. A 1]----------------------------------------------
                    // [. A 1]: action: ( insert )
                    // iC = I [iA] ; or I is a colon expression
                    int64_t iC = GB_ijlist (I, iA, Ikind, Icolon) ;
                    GB_D_A_1_matrix ;
                }
                GB_NEXT (A) ;
            }
        }

        //----------------------------------------------------------------------
        // log the result of this task
        //----------------------------------------------------------------------

        ok = ok && task_ok ;
    }

    //--------------------------------------------------------------------------
    // finalize the matrix and return result
    //--------------------------------------------------------------------------

    GB_SUBASSIGN_WRAPUP ;
}
