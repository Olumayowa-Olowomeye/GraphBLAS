
// Decide branch direction for GPU use for the dot-product MxM
extern "C" 
{
  #include "GB_mxm.h"
}
#include "GB_cuda.h"
#include <cuda_runtime.h>

bool GB_AxB_dot3_cuda_branch 
(
    const GrB_Matrix M,             // mask matrix
    const bool Mask_struct,         // if true, use the only structure of M
    const GrB_Matrix A,             // input matrix
    const GrB_Matrix B,             // input matrix
    const GrB_Semiring semiring,    // semiring that defines C=A*B
    const bool flipxy,              // if true, do z=fmult(b,a) vs fmult(a,b)
    GB_Context Context
)
{
        // very rough estimate of the work to do
        double adeg = ((double) GB_nnz (A)) / ((double) GB_IMAX (1, A->nvec)) ;
        double bdeg = ((double) GB_nnz (B)) / ((double) GB_IMAX (1, B->nvec)) ;
        double work = GB_nnz (M) * GB_IMIN (adeg, bdeg) ;

        // TODO if A or B are not accessed (first, 2nd, or pair ops)
        // then the type if A can be user-defined here, for CUDA.

        // TODO: the test for a built-in semiring needs to be
        // removed, to allow for the generation of CUDA kernels for non-
        // built-in semirings.  The code generation process currently does not
        // support user-defined types and operators, but this needs to be
        // handled.  In addition, CUDA kernels could be built for semirings
        // that are not built-in, but consist solely of built-in types and
        // operators (such as BOR_BSHIFT on INT32 inputs).

        int ngpus_to_use = GB_ngpus_to_use (work) ;
        GBURBLE (" work:%g GPUs:%d ", work, ngpus_to_use) ;
        if (ngpus_to_use > 0
            // FIXME: FUTURE: user-defined types and operators
            && (A->type->code != GB_UDT_code)
            && (B->type->code != GB_UDT_code)
            // FIXME: handle A, B bitmap and/or full
            && !GB_IS_BITMAP (A) && !GB_IS_BITMAP (B)
            && !GB_IS_FULL (A) && !GB_IS_FULL (B))
        {
            return true;
        }
        else
        {
            // FIXME: remove debug outout here:
            std::cout << "Not using cuda path. M_is_hypersparse: " << GB_IS_HYPERSPARSE(M) <<
                         ", A->iso: " << A->iso << ", B->iso: " << B->iso << ", A_BITMAP: " << GB_IS_BITMAP(A) <<
                         ", B_BITMAP: " << GB_IS_BITMAP(B) << ", GB_IS_FULL(A): " << GB_IS_FULL(A)
                         << ", GB_IS_FULL(B): " << GB_IS_FULL(B) << ", semiring header size: " << semiring->header_size <<

                         std::endl;

            return false;
        }

}
