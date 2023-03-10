//------------------------------------------------------------------------------
// GB_selector:  select entries from a matrix
//------------------------------------------------------------------------------

// SuiteSparse:GraphBLAS, Timothy A. Davis, (c) 2017-2022, All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

//------------------------------------------------------------------------------

// GB_selector does the work for GB_select.  It also deletes zombies for
// GB_wait using the GxB_NONZOMBIE operator, and deletes entries outside a
// smaller matrix for GxB_*resize.

// TODO: GB_selector does not exploit the mask.

// If C is NULL on input, A is modified in-place.
// Otherwise, C is an uninitialized static header.

#define GB_DEBUG

#include "GB_select.h"
#include "GB_ek_slice.h"
#ifndef GBCUDA_DEV
#include "GB_sel__include.h"
#endif
#include "GB_scalar.h"
#include "GB_transpose.h"
#include "GB_stringify.h"

#define GB_FREE_WORKSPACE                   \
{                                           \
    GB_FREE_WORK (&Zp, Zp_size) ;           \
    GB_WERK_POP (Work, int64_t) ;           \
    GB_WERK_POP (A_ek_slicing, int64_t) ;   \
    GB_FREE (&Cp, Cp_size) ;                \
    GB_FREE (&Ch, Ch_size) ;                \
    GB_FREE (&Ci, Ci_size) ;                \
    GB_FREE (&Cx, Cx_size) ;                \
}

#define GB_FREE_ALL                         \
{                                           \
    GB_phybix_free (C) ;                    \
    GB_FREE_WORKSPACE ;                     \
}

GrB_Info GB_selector
(
    GrB_Matrix C,               // output matrix, NULL or existing header
    const GrB_IndexUnaryOp op,
    const bool flipij,          // if true, flip i and j for user operator
    GrB_Matrix A,               // input matrix
    const GrB_Scalar Thunk,
    GB_Werk Werk
)
{

    //--------------------------------------------------------------------------
    // check inputs
    //--------------------------------------------------------------------------

    GrB_Info info ;
    ASSERT_INDEXUNARYOP_OK (op, "idxunop for GB_selector", GB0) ;
    ASSERT_SCALAR_OK (Thunk, "Thunk for GB_selector", GB0) ;
    ASSERT_MATRIX_OK (A, "A input for GB_selector", GB_FLIP (GB0)) ;
    // positional selector (tril, triu, diag, offdiag, resize, rowindex, ...):
    // can't be jumbled.  nonzombie, entry-valued op, user op: jumbled OK
    GB_Opcode opcode = op->opcode ;
    ASSERT (GB_IMPLIES (GB_OPCODE_IS_POSITIONAL (opcode), !GB_JUMBLED (A))) ;
    ASSERT (C == NULL || (C != NULL && (C->static_header || GBNSTATIC))) ;

    //--------------------------------------------------------------------------
    // declare workspace
    //--------------------------------------------------------------------------

    bool in_place_A = (C == NULL) ; // GrB_wait and GB_resize only
    int64_t *restrict Zp = NULL ; size_t Zp_size = 0 ;
    GB_WERK_DECLARE (Work, int64_t) ;
    int64_t *restrict Wfirst = NULL ;
    int64_t *restrict Wlast = NULL ;
    int64_t *restrict Cp_kfirst = NULL ;
    GB_WERK_DECLARE (A_ek_slicing, int64_t) ;

    int64_t avlen = A->vlen ;
    int64_t avdim = A->vdim ;
    const bool A_iso = A->iso ;

    int64_t *restrict Cp = NULL ; size_t Cp_size = 0 ;
    int64_t *restrict Ch = NULL ; size_t Ch_size = 0 ;
    int64_t *restrict Ci = NULL ; size_t Ci_size = 0 ;
    GB_void *restrict Cx = NULL ; size_t Cx_size = 0 ;

    //--------------------------------------------------------------------------
    // get Thunk
    //--------------------------------------------------------------------------

    const size_t asize = A->type->size ;
    const GB_Type_code acode = A->type->code ;

    GrB_Type xtype = NULL ;
    GB_Type_code xcode = GB_ignore_code ;
    size_t xsize = 1 ;

    // get the type of the thunk input of the operator
    GrB_Type ytype = op->ytype ;
    GB_Type_code ycode = ytype->code ;
    size_t ysize = ytype->size ;
    if (op->xtype != NULL)
    { 
        // get the type of the A input of the operator
        xtype = op->xtype ;
        xcode = xtype->code ;
        xsize = xtype->size ;
    }

    bool op_is_positional = GB_IS_INDEXUNARYOP_CODE_POSITIONAL (opcode) ;

    ASSERT (GB_nnz ((GrB_Matrix) Thunk) > 0) ;
    const GB_Type_code tcode = Thunk->type->code ;

    // ythunk = (op->ytype) Thunk
    GB_void ythunk [GB_VLA(ysize)] ;
    memset (ythunk, 0, ysize) ;
    GB_cast_scalar (ythunk, ycode, Thunk->x, tcode, ysize) ;

    // ithunk = (int64) Thunk, if compatible
    int64_t ithunk = 0 ;
    if (GB_Type_compatible (GrB_INT64, Thunk->type))
    {
        GB_cast_scalar (&ithunk, GB_INT64_code, Thunk->x, tcode,
            sizeof (int64_t)) ;
    }

    // athunk = (A->type) Thunk, for VALUEEQ operator only
    GB_void athunk [GB_VLA(asize)] ;
    memset (athunk, 0, asize) ;
    if (opcode == GB_VALUEEQ_idxunop_code)
    {
        ASSERT (GB_Type_compatible (A->type, Thunk->type)) ;
        GB_cast_scalar (athunk, acode, Thunk->x, tcode, asize) ;
    }

    //--------------------------------------------------------------------------
    // handle iso case for built-in ops that depend only on the value
    //--------------------------------------------------------------------------

    if (A_iso && opcode >= GB_VALUENE_idxunop_code
              && opcode <= GB_VALUELE_idxunop_code)
    { 

        // The VALUE* operators depend only on the value of A(i,j).  Since A is
        // iso, either all entries in A will be copied to C and thus C can be
        // created as a shallow copy of A, or no entries from A will be copied
        // to C and thus C is an empty matrix.  The select factory is not
        // needed, except to check the iso value via GB_selector_bitmap.

        ASSERT (!in_place_A) ;
        ASSERT (C != NULL && (C->static_header || GBNSTATIC)) ;

        // construct a scalar S containing the iso scalar of ((xtype) A)
        struct GB_Scalar_opaque S_header ;
        GrB_Scalar S ;
        // wrap the iso-value of A in the scalar S, typecasted to xtype
        // xscalar = (op->xtype) A->x
        GB_void xscalar [GB_VLA(xsize)] ;
        GB_cast_scalar (xscalar, xcode, A->x, acode, asize) ;
        S = GB_Scalar_wrap (&S_header, xtype, xscalar) ;
        S->iso = false ;    // but ensure S is not iso
        ASSERT_SCALAR_OK (S, "iso scalar wrap", GB0) ;

        // apply the select operator to the iso scalar S
        GB_OK (GB_selector_bitmap (C, false, op, false,
            (GrB_Matrix) S, ithunk, athunk, ythunk, Werk)) ;
        ASSERT_MATRIX_OK (C, "C from iso scalar test", GB0) ;
        bool C_empty = (GB_nnz (C) == 0) ;
        GB_phybix_free (C) ;

        // check if C has 0 or 1 entry
        if (C_empty)
        { 
            // C is an empty matrix
            return (GB_new (&C, // existing header
                A->type, avlen, avdim, GB_Ap_calloc, true,
                GxB_AUTO_SPARSITY, GB_Global_hyper_switch_get ( ), 1)) ;
        }
        else
        { 
            // C is a shallow copy of A with all the same entries as A
            // set C->iso = A->iso  OK
            return (GB_shallow_copy (C, true, A, Werk)) ;
        }
    }

    //--------------------------------------------------------------------------
    // determine if C is iso for a non-iso A
    //--------------------------------------------------------------------------

    bool C_iso = A_iso ||                       // C iso value is Ax [0]
        (opcode == GB_VALUEEQ_idxunop_code) ;   // C iso value is thunk

    if (C_iso)
    { 
        GB_BURBLE_MATRIX (A, "(iso select) ") ;
    }

    // The CUDA select kernel might be called here?

    //--------------------------------------------------------------------------
    // handle the bitmap/as-if-full case
    //--------------------------------------------------------------------------

    bool use_selector_bitmap ;
    if (opcode == GB_NONZOMBIE_idxunop_code || in_place_A)
    { 
        // GB_selector_bitmap does not support the nonzombie opcode, nor does
        // it support operating on A in place.  For the NONZOMBIE operator, A
        // will never be bitmap.
        use_selector_bitmap = false ;
    }
    else if (opcode == GB_DIAG_idxunop_code)
    { 
        // GB_selector_bitmap supports the DIAG operator, but it is currently
        // not efficient (GB_selector_bitmap should return a sparse diagonal
        // matrix, not bitmap).  So use the sparse case if A is not bitmap,
        // since the sparse case below does not support the bitmap case.
        use_selector_bitmap = GB_IS_BITMAP (A) ;
    }
    else
    { 
        // For bitmap, full, or as-if-full matrices (sparse/hypersparse with
        // all entries present, not jumbled, no zombies, and no pending
        // tuples), use the bitmap selector for all other operators (TRIL,
        // TRIU, OFFDIAG, NONZERO, EQ*, GT*, GE*, LT*, LE*, and user-defined
        // operators).
        use_selector_bitmap = GB_IS_BITMAP (A) || GB_as_if_full (A) ;
    }

    //--------------------------------------------------------------------------

    // The CUDA select kernel would be called here at the latest.

    //==========================================================================
    // bitmap/full case
    //==========================================================================

    if (use_selector_bitmap)
    { 
        GB_BURBLE_MATRIX (A, "(bitmap select) ") ;
        ASSERT (C != NULL && (C->static_header || GBNSTATIC)) ;
        return (GB_selector_bitmap (C, C_iso, op,                  
            flipij, A, ithunk, athunk, ythunk, Werk)) ;
    }

    //==========================================================================
    // sparse/hypersparse case
    //==========================================================================

    //--------------------------------------------------------------------------
    // determine the max number of threads to use
    //--------------------------------------------------------------------------

    int nthreads_max = GB_Context_nthreads_max ( ) ;
    double chunk = GB_Context_chunk ( ) ;

    //--------------------------------------------------------------------------
    // get A: sparse, hypersparse, or full
    //--------------------------------------------------------------------------

    // the case when A is bitmap is always handled above by GB_selector_bitmap
    ASSERT (!GB_IS_BITMAP (A)) ;

    int64_t *restrict Ap = A->p ; size_t Ap_size = A->p_size ;
    int64_t *restrict Ah = A->h ;
    int64_t *restrict Ai = A->i ; size_t Ai_size = A->i_size ;
    GB_void *restrict Ax = (GB_void *) A->x ; size_t Ax_size = A->x_size ;
    int64_t anvec = A->nvec ;
    bool A_jumbled = A->jumbled ;
    bool A_is_hyper = (Ah != NULL) ;

    //==========================================================================
    // column selector
    //==========================================================================

    // The column selectors can be done in a single pass.
    // FIXME: put this in its own function.

    if (opcode == GB_COLINDEX_idxunop_code ||
        opcode == GB_COLLE_idxunop_code ||
        opcode == GB_COLGT_idxunop_code)
    {

        //----------------------------------------------------------------------
        // find column j in A
        //----------------------------------------------------------------------

        ASSERT_MATRIX_OK (A, "A for col selector", GB_FLIP (GB0)) ;
        int nth = nthreads_max ;
        ASSERT (!in_place_A) ;
        ASSERT (C != NULL && (C->static_header || GBNSTATIC)) ;
        ASSERT (GB_JUMBLED_OK (A)) ;

        int64_t j = (opcode == GB_COLINDEX_idxunop_code) ? (-ithunk) : ithunk ;

        int64_t k = 0 ;
        bool found ;
        if (j < 0)
        { 
            // j is outside the range of columns of A
            k = 0 ;
            found = false ;
        }
        else if (j >= avdim)
        { 
            // j is outside the range of columns of A
            k = anvec ;
            found = false ;
        }
        else if (A_is_hyper)
        { 
            // find the column j in the hyperlist of A
            // FIXME: use hyperhash if present
            int64_t kright = anvec-1 ;
            GB_SPLIT_BINARY_SEARCH (j, Ah, k, kright, found) ;
            // if found is true the Ah [k] == j
            // if found is false, then Ah [0..k-1] < j and Ah [k..anvec-1] > j
        }
        else
        { 
            // j appears as the jth column in A; found is always true
            k = j ;
            found = true ;
        }

        //----------------------------------------------------------------------
        // determine the # of entries and # of vectors in C
        //----------------------------------------------------------------------

        int64_t pstart = Ap [k] ;
        int64_t pend = found ? Ap [k+1] : pstart ;
        int64_t ajnz = pend - pstart ;
        int64_t cnz, cnvec ;
        int64_t anz = Ap [anvec] ;

        if (opcode == GB_COLINDEX_idxunop_code)
        { 
            // COLINDEX: delete column j:  C = A (:, [0:j-1 j+1:end])
            cnz = anz - ajnz ;
            cnvec = (A_is_hyper && found) ? (anvec-1) : anvec ;
        }
        else if (opcode == GB_COLLE_idxunop_code)
        { 
            // COLLE: C = A (:, 0:j)
            cnz = pend ;
            cnvec = (A_is_hyper) ? (found ? (k+1) : k) : anvec ;
        }
        else // (opcode == GB_COLGT_idxunop_code)
        { 
            // COLGT: C = A (:, j+1:end)
            cnz = anz - pend ;
            cnvec = anvec - ((A_is_hyper) ? (found ? (k+1) : k) : 0) ;
        }

        if (cnz == anz)
        { 
            // C is the same as A: return it a pure shallow copy
            return (GB_shallow_copy (C, true, A, Werk)) ;
        }
        else if (cnz == 0)
        { 
            // return C as empty
            return (GB_new (&C, // auto (sparse or hyper), existing header
                A->type, avlen, avdim, GB_Ap_calloc, true,
                GxB_AUTO_SPARSITY, GB_Global_hyper_switch_get ( ), 1)) ;
        }

        //----------------------------------------------------------------------
        // allocate C
        //----------------------------------------------------------------------

        int csparsity = (A_is_hyper) ? GxB_HYPERSPARSE : GxB_SPARSE ;
        GB_OK (GB_new_bix (&C, // sparse or hyper (from A), existing header
            A->type, avlen, avdim, GB_Ap_malloc, true, csparsity, false,
            A->hyper_switch, cnvec, cnz, true, A_iso)) ;

        ASSERT (info == GrB_SUCCESS) ;
        int nth2 = GB_nthreads (cnvec, chunk, nth) ;

        int64_t *restrict Cp = C->p ;
        int64_t *restrict Ch = C->h ;
        int64_t *restrict Ci = C->i ;
        GB_void *restrict Cx = (GB_void *) C->x ;
        int64_t kk ;

        //----------------------------------------------------------------------
        // construct C
        //----------------------------------------------------------------------

        if (A_iso)
        { 
            // Cx [0] = Ax [0]
            memcpy (Cx, Ax, asize) ;
        }

        if (opcode == GB_COLINDEX_idxunop_code)
        {

            //------------------------------------------------------------------
            // COLINDEX: delete the column j
            //------------------------------------------------------------------

            if (A_is_hyper)
            { 
                ASSERT (found) ;
                // Cp [0:k-1] = Ap [0:k-1]
                GB_memcpy (Cp, Ap, k * sizeof (int64_t), nth) ;
                // Cp [k:cnvec] = Ap [k+1:anvec] - ajnz
                #pragma omp parallel for num_threads(nth2)
                for (kk = k ; kk <= cnvec ; kk++)
                { 
                    Cp [kk] = Ap [kk+1] - ajnz ;
                }
                // Ch [0:k-1] = Ah [0:k-1]
                GB_memcpy (Ch, Ah, k * sizeof (int64_t), nth) ;
                // Ch [k:cnvec-1] = Ah [k+1:anvec-1]
                GB_memcpy (Ch + k, Ah + (k+1), (cnvec-k) * sizeof (int64_t),
                    nth) ;
            }
            else
            { 
                // Cp [0:k] = Ap [0:k]
                GB_memcpy (Cp, Ap, (k+1) * sizeof (int64_t), nth) ;
                // Cp [k+1:anvec] = Ap [k+1:anvec] - ajnz
                #pragma omp parallel for num_threads(nth2)
                for (kk = k+1 ; kk <= cnvec ; kk++)
                { 
                    Cp [kk] = Ap [kk] - ajnz ;
                }
            }
            // Ci [0:pstart-1] = Ai [0:pstart-1]
            GB_memcpy (Ci, Ai, pstart * sizeof (int64_t), nth) ;
            // Ci [pstart:cnz-1] = Ai [pend:anz-1]
            GB_memcpy (Ci + pstart, Ai + pend,
                (cnz - pstart) * sizeof (int64_t), nth) ;
            if (!A_iso)
            { 
                // Cx [0:pstart-1] = Ax [0:pstart-1]
                GB_memcpy (Cx, Ax, pstart * asize, nth) ;
                // Cx [pstart:cnz-1] = Ax [pend:anz-1]
                GB_memcpy (Cx + pstart * asize, Ax + pend * asize,
                    (cnz - pstart) * asize, nth) ;
            }

        }
        else if (opcode == GB_COLLE_idxunop_code)
        {

            //------------------------------------------------------------------
            // COLLE: C = A (:, 0:j)
            //------------------------------------------------------------------

            if (A_is_hyper)
            { 
                // Cp [0:cnvec] = Ap [0:cnvec]
                GB_memcpy (Cp, Ap, (cnvec+1) * sizeof (int64_t), nth) ;
                // Ch [0:cnvec-1] = Ah [0:cnvec-1]
                GB_memcpy (Ch, Ah, (cnvec) * sizeof (int64_t), nth) ;
            }
            else
            {
                // Cp [0:k+1] = Ap [0:k+1]
                ASSERT (found) ;
                GB_memcpy (Cp, Ap, (k+2) * sizeof (int64_t), nth) ;
                // Cp [k+2:cnvec] = cnz
                #pragma omp parallel for num_threads(nth2)
                for (kk = k+2 ; kk <= cnvec ; kk++)
                { 
                    Cp [kk] = cnz ;
                }
            }
            // Ci [0:cnz-1] = Ai [0:cnz-1]
            GB_memcpy (Ci, Ai, cnz * sizeof (int64_t), nth) ;
            if (!A_iso)
            { 
                // Cx [0:cnz-1] = Ax [0:cnz-1]
                GB_memcpy (Cx, Ax, cnz * asize, nth) ;
            }

        }
        else // (opcode == GB_COLGT_idxunop_code)
        {

            //------------------------------------------------------------------
            // COLGT: C = A (:, j+1:end)
            //------------------------------------------------------------------

            if (A_is_hyper)
            { 
                // Cp [0:cnvec] = Ap [k+found:anvec] - pend
                #pragma omp parallel for num_threads(nth2)
                for (kk = 0 ; kk <= cnvec ; kk++)
                { 
                    Cp [kk] = Ap [kk + k + found] - pend ;
                }
                // Ch [0:cnvec-1] = Ah [k+found:anvec-1]
                GB_memcpy (Ch, Ah + k + found, cnvec * sizeof (int64_t), nth) ;
            }
            else
            {
                ASSERT (found) ;
                // Cp [0:k] = 0
                GB_memset (Cp, 0, (k+1) * sizeof (int64_t), nth) ;
                // Cp [k+1:cnvec] = Ap [k+1:cnvec] - pend
                #pragma omp parallel for num_threads(nth2)
                for (kk = k+1 ; kk <= cnvec ; kk++)
                { 
                    Cp [kk] = Ap [kk] - pend ;
                }
            }
            // Ci [0:cnz-1] = Ai [pend:anz-1]
            GB_memcpy (Ci, Ai + pend, cnz * sizeof (int64_t), nth) ;
            if (!A_iso)
            { 
                // Cx [0:cnz-1] = Ax [pend:anz-1]
                GB_memcpy (Cx, Ax + pend * asize, cnz * asize, nth) ;
            }
        }

        //----------------------------------------------------------------------
        // finalize the matrix, free workspace, and return result
        //----------------------------------------------------------------------

        C->nvec = cnvec ;
        C->magic = GB_MAGIC ;
        C->jumbled = A_jumbled ;    // C is jumbled if A is jumbled
        C->iso = C_iso ;            // OK: burble already done above
        C->nvals = Cp [cnvec] ;
        C->nvec_nonempty = GB_nvec_nonempty (C) ;
        ASSERT_MATRIX_OK (C, "C output for GB_selector (column select)", GB0) ;
        return (GrB_SUCCESS) ;
    }

    //==========================================================================
    // all other operators
    //==========================================================================

    #undef  GB_FREE_ALL
    #define GB_FREE_ALL                         \
    {                                           \
        GB_phybix_free (C) ;                    \
        GB_FREE_WORKSPACE ;                     \
    }

    //--------------------------------------------------------------------------
    // allocate the new vector pointers of C
    //--------------------------------------------------------------------------

    int64_t cnz = 0 ;
    int64_t cplen = GB_IMAX (1, anvec) ;

    Cp = GB_CALLOC (cplen+1, int64_t, &Cp_size) ;
    if (Cp == NULL)
    { 
        // out of memory
        return (GrB_OUT_OF_MEMORY) ;
    }

    //--------------------------------------------------------------------------
    // slice the entries for each task
    //--------------------------------------------------------------------------

    int A_ntasks, A_nthreads ;
    double work = 8*anvec
        + ((opcode == GB_DIAG_idxunop_code) ? 0 : GB_nnz_held (A)) ;
    GB_SLICE_MATRIX_WORK (A, 8, chunk, work) ;

    //--------------------------------------------------------------------------
    // allocate workspace for each task
    //--------------------------------------------------------------------------

    GB_WERK_PUSH (Work, 3*A_ntasks, int64_t) ;
    if (Work == NULL)
    { 
        // out of memory
        GB_FREE_ALL ;
        return (GrB_OUT_OF_MEMORY) ;
    }
    Wfirst    = Work ;
    Wlast     = Work + A_ntasks ;
    Cp_kfirst = Work + A_ntasks * 2 ;

    //--------------------------------------------------------------------------
    // allocate workspace for phase1
    //--------------------------------------------------------------------------

    // phase1 counts the number of live entries in each vector of A.  The
    // result is computed in Cp, where Cp [k] is the number of live entries in
    // the kth vector of A.  Zp [k] is the location of the A(i,k) entry, for
    // positional operators.

    if (op_is_positional)
    {
        // allocate Zp
        Zp = GB_MALLOC_WORK (cplen, int64_t, &Zp_size) ;
        if (Zp == NULL)
        { 
            // out of memory
            GB_FREE_ALL ;
            return (GrB_OUT_OF_MEMORY) ;
        }
    }

    //==========================================================================
    // phase1: count the live entries in each column
    //==========================================================================

    info = GrB_NO_VALUE ;
    if (op_is_positional || opcode == GB_NONZOMBIE_idxunop_code)
    {

        //----------------------------------------------------------------------
        // positional ops or nonzombie phase1 do not depend on the values
        //----------------------------------------------------------------------

        // no JIT worker needed for these operators
        info = GB_select_positional_phase1 (Zp, Cp, Wfirst, Wlast, A, ithunk,
            op, A_ek_slicing, A_ntasks, A_nthreads) ;

    }
    else
    {

        //----------------------------------------------------------------------
        // entry selectors depend on the values in phase1
        //----------------------------------------------------------------------

        ASSERT (!A_iso || opcode == GB_USER_idxunop_code) ;
        ASSERT ((opcode >= GB_VALUENE_idxunop_code
             && opcode <= GB_VALUELE_idxunop_code)
             || (opcode == GB_USER_idxunop_code)) ;

        #ifndef GBCUDA_DEV

            //------------------------------------------------------------------
            // via the factory kernel (includes user-defined ops)
            //------------------------------------------------------------------

            // define the worker for the switch factory
            #define GB_sel1(opname,aname) GB (_sel_phase1_ ## opname ## aname)
            #define GB_SEL_WORKER(opname,aname)                             \
            {                                                               \
                info = GB_sel1 (opname, aname) (Cp, Wfirst, Wlast, A,       \
                    ythunk, op, A_ek_slicing, A_ntasks, A_nthreads) ;       \
            }                                                               \
            break ;

            // launch the switch factory
            #include "GB_select_entry_factory.c"
            #undef  GB_SEL_WORKER

        #endif

        #if GB_JIT_ENABLED
        // JIT TODO: select: phase1 entry selectors
        #endif

        if (info == GrB_NO_VALUE)
        {
            // generic entry selector, phase1
            info = GB_select_generic_phase1 (Cp, Wfirst, Wlast, A,
                flipij, ythunk, op, A_ek_slicing, A_ntasks, A_nthreads) ;
        }
    }

    //==========================================================================
    // phase1b: cumulative sum and allocate C
    //==========================================================================

    //--------------------------------------------------------------------------
    // cumulative sum of Cp and compute Cp_kfirst
    //--------------------------------------------------------------------------

    int64_t C_nvec_nonempty ;
    GB_ek_slice_merge2 (&C_nvec_nonempty, Cp_kfirst, Cp, anvec,
        Wfirst, Wlast, A_ek_slicing, A_ntasks, A_nthreads, Werk) ;

    //--------------------------------------------------------------------------
    // allocate new space for the compacted Ci and Cx
    //--------------------------------------------------------------------------

    cnz = Cp [anvec] ;
    cnz = GB_IMAX (cnz, 1) ;
    Ci = GB_MALLOC (cnz, int64_t, &Ci_size) ;
    // use calloc since C is sparse, not bitmap
    Cx = (GB_void *) GB_XALLOC (false, C_iso, cnz, asize, &Cx_size) ; // x:OK
    if (Ci == NULL || Cx == NULL)
    { 
        // out of memory
        GB_FREE_ALL ;
        return (GrB_OUT_OF_MEMORY) ;
    }

    //--------------------------------------------------------------------------
    // set the iso value of C
    //--------------------------------------------------------------------------

    if (C_iso)
    { 
        // The pattern of C is computed by the worker below.
        GB_select_iso (Cx, opcode, athunk, Ax, asize) ;
    }

    //==========================================================================
    // phase2: select the entries
    //==========================================================================

    info = GrB_NO_VALUE ;
    if (op_is_positional || (opcode == GB_NONZOMBIE_idxunop_code && A_iso))
    {

        //----------------------------------------------------------------------
        // positional ops do not depend on the values
        //----------------------------------------------------------------------

        // no JIT worker needed for these operators
        info = GB_select_positional_phase2 (Ci, Cx, Zp, Cp, Cp_kfirst, A,
            flipij, ithunk, op, A_ek_slicing, A_ntasks, A_nthreads) ;

    }
    else
    {

        //----------------------------------------------------------------------
        // entry selectors depend on the values in phase2
        //----------------------------------------------------------------------

        ASSERT (!A_iso || opcode == GB_USER_idxunop_code) ;
        ASSERT ((opcode >= GB_VALUENE_idxunop_code &&
                 opcode <= GB_VALUELE_idxunop_code)
             || (opcode == GB_NONZOMBIE_idxunop_code && !A_iso)
             || (opcode == GB_USER_idxunop_code)) ;

        #ifndef GBCUDA_DEV

            //------------------------------------------------------------------
            // via the factory kernel
            //------------------------------------------------------------------

            // define the worker for the switch factory
            #define GB_SELECT_PHASE2
            #define GB_sel2(opname,aname) GB (_sel_phase2_ ## opname ## aname)
            #define GB_SEL_WORKER(opname,aname)                             \
            {                                                               \
                info = GB_sel2 (opname, aname) (Ci, Cx, Cp, Cp_kfirst, A,   \
                    ythunk, op, A_ek_slicing, A_ntasks, A_nthreads) ;       \
            }                                                               \
            break ;

            // launch the switch factory
            #include "GB_select_entry_factory.c"

        #endif

        //----------------------------------------------------------------------
        // via the JIT kernel
        //----------------------------------------------------------------------

        #if GB_JIT_ENABLED
        // JIT TODO: select: phase2
        #endif

        if (info == GrB_NO_VALUE)
        {
            // generic entry selector, phase2
            info = GB_select_generic_phase2 (Ci, Cx, Cp, Cp_kfirst, A, flipij,
                ythunk, op, A_ek_slicing, A_ntasks, A_nthreads) ;
        }
    }

    //==========================================================================
    // finalize the result
    //==========================================================================

    if (in_place_A)
    {

        //----------------------------------------------------------------------
        // transplant Cp, Ci, Cx back into A
        //----------------------------------------------------------------------

        // TODO: this is not parallel: use GB_hyper_prune
        if (A->h != NULL && C_nvec_nonempty < anvec)
        {
            // prune empty vectors from Ah and Ap
            int64_t cnvec = 0 ;
            for (int64_t k = 0 ; k < anvec ; k++)
            {
                if (Cp [k] < Cp [k+1])
                { 
                    Ah [cnvec] = Ah [k] ;
                    Ap [cnvec] = Cp [k] ;
                    cnvec++ ;
                }
            }
            Ap [cnvec] = Cp [anvec] ;
            A->nvec = cnvec ;
            ASSERT (A->nvec == C_nvec_nonempty) ;
            GB_FREE (&Cp, Cp_size) ;
            // the A->Y hyper_hash is now invalid
            GB_hyper_hash_free (A) ;
        }
        else
        { 
            // free the old A->p and transplant in Cp as the new A->p
            GB_FREE (&Ap, Ap_size) ;
            A->p = Cp ; Cp = NULL ; A->p_size = Cp_size ;
            A->plen = cplen ;
        }

        ASSERT (Cp == NULL) ;

        GB_FREE (&Ai, Ai_size) ;
        GB_FREE (&Ax, Ax_size) ;
        A->i = Ci ; Ci = NULL ; A->i_size = Ci_size ;
        A->x = Cx ; Cx = NULL ; A->x_size = Cx_size ;
        A->nvec_nonempty = C_nvec_nonempty ;
        A->jumbled = A_jumbled ;        // A remains jumbled (in-place select)
        A->iso = C_iso ;                // OK: burble already done above
        A->nvals = A->p [A->nvec] ;

        // the NONZOMBIE opcode may have removed all zombies, but A->nzombie
        // is still nonzero.  It is set to zero in GB_wait.
        ASSERT_MATRIX_OK (A, "A output for GB_selector", GB_FLIP (GB0)) ;

    }
    else
    {

        //----------------------------------------------------------------------
        // create C and transplant Cp, Ch, Ci, Cx into C
        //----------------------------------------------------------------------

        int csparsity = (A_is_hyper) ? GxB_HYPERSPARSE : GxB_SPARSE ;
        ASSERT (C != NULL && (C->static_header || GBNSTATIC)) ;
        info = GB_new (&C, // sparse or hyper (from A), existing header
            A->type, avlen, avdim, GB_Ap_null, true,
            csparsity, A->hyper_switch, anvec) ;
        ASSERT (info == GrB_SUCCESS) ;

        if (A->h != NULL)
        {

            //------------------------------------------------------------------
            // A and C are hypersparse: copy non-empty vectors from Ah to Ch
            //------------------------------------------------------------------

            Ch = GB_MALLOC (anvec, int64_t, &Ch_size) ;
            if (Ch == NULL)
            { 
                // out of memory
                GB_FREE_ALL ;
                return (GrB_OUT_OF_MEMORY) ;
            }

            // TODO: do in parallel: use GB_hyper_prune
            int64_t cnvec = 0 ;
            for (int64_t k = 0 ; k < anvec ; k++)
            {
                if (Cp [k] < Cp [k+1])
                { 
                    Ch [cnvec] = Ah [k] ;
                    Cp [cnvec] = Cp [k] ;
                    cnvec++ ;
                }
            }
            Cp [cnvec] = Cp [anvec] ;
            C->nvec = cnvec ;
            ASSERT (C->nvec == C_nvec_nonempty) ;
        }

        // note that C->Y is not yet constructed
        C->p = Cp ; Cp = NULL ; C->p_size = Cp_size ;
        C->h = Ch ; Ch = NULL ; C->h_size = Ch_size ;
        C->i = Ci ; Ci = NULL ; C->i_size = Ci_size ;
        C->x = Cx ; Cx = NULL ; C->x_size = Cx_size ;
        C->plen = cplen ;
        C->magic = GB_MAGIC ;
        C->nvec_nonempty = C_nvec_nonempty ;
        C->jumbled = A_jumbled ;    // C is jumbled if A is jumbled
        C->iso = C_iso ;            // OK: burble already done above
        C->nvals = C->p [C->nvec] ;

        ASSERT_MATRIX_OK (C, "C output for GB_selector", GB0) ;
    }

    //--------------------------------------------------------------------------
    // free workspace and return result
    //--------------------------------------------------------------------------

    GB_FREE_WORKSPACE ;
    return (GrB_SUCCESS) ;
}

