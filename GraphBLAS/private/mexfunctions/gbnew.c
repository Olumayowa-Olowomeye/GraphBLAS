//------------------------------------------------------------------------------
// gbnew: create a GraphBLAS matrix
//------------------------------------------------------------------------------

// SuiteSparse:GraphBLAS, Timothy A. Davis, (c) 2017-2019, All Rights Reserved.
// http://suitesparse.com   See GraphBLAS/Doc/License.txt for license.

//------------------------------------------------------------------------------

// X may be a MATLAB sparse matrix, or a MATLAB struct containing a GraphBLAS
// matrix.  A is returned as a MATLAB struct containing a GraphBLAS matrix.

#include "gb_matlab.h"

void mexFunction
(
    int nargout,
    mxArray *pargout [ ],
    int nargin,
    const mxArray *pargin [ ]
)
{

    //--------------------------------------------------------------------------
    // check inputs
    //--------------------------------------------------------------------------

    gb_usage (nargin <= 3 && nargout <= 1,
        "usage: A = gb (m,n,type) or A = gb (X,type)") ;

    //--------------------------------------------------------------------------
    // construct the GraphBLAS matrix
    //--------------------------------------------------------------------------

    GrB_Matrix A ;

    if (nargin == 0)
    {

        //----------------------------------------------------------------------
        // A = gbnew ; empty 1-by-1 GraphBLAS double matrix
        //----------------------------------------------------------------------

        OK (GrB_Matrix_new (&A, GrB_FP64, 1, 1)) ;

    }
    else if (nargin == 1)
    {

        if (mxIsChar (pargin [0]))
        {

            //------------------------------------------------------------------
            // A = gb (type) ; empty 1-by-1 GraphBLAS matrix of given type
            //------------------------------------------------------------------

            OK (GrB_Matrix_new (&A, gb_mxstring_to_type (pargin [0]), 1, 1)) ;

        }
        else
        {

            //------------------------------------------------------------------
            // A = gb (X) ; GraphBLAS copy of X, same type
            //------------------------------------------------------------------

            // X can be a MATLAB sparse or dense matrix, or a GraphBLAS struct

            A = gb_get_deep (pargin [0], NULL) ;

        }

    }
    else if (nargin == 2)
    {

        if (mxIsChar (pargin [1]))
        {

            //------------------------------------------------------------------
            // A = gb (X, type) ; GraphBLAS typecasted copy of MATLAB X
            //------------------------------------------------------------------

            A = gb_get_deep (pargin [0], gb_mxstring_to_type (pargin [1])) ;

        }
        else if (gb_mxarray_is_scalar (pargin [0]) &&
                 gb_mxarray_is_scalar (pargin [1]))
        {

            //------------------------------------------------------------------
            // A = gb (m, n) ; empty m-by-n GraphBLAS double matrix
            //------------------------------------------------------------------

            OK (GrB_Matrix_new (&A, GrB_FP64,
                (int64_t) mxGetScalar (pargin [0]) , 
                (int64_t) mxGetScalar (pargin [1]))) ;

        }
        else
        {

            USAGE ("usage: A = gb (m,n) or A = gb (X,type)") ;
        }

    }
    else if (nargin == 3)
    {

        //----------------------------------------------------------------------
        // A = gb (m, n, type) ; empty m-by-n GraphBLAS matrix of given type
        //----------------------------------------------------------------------

        if (!gb_mxarray_is_scalar (pargin [0]) ||
            !gb_mxarray_is_scalar (pargin [1]) || 
            !mxIsChar (pargin [2]))
        {
            USAGE ("usage: A = gb (m,n,type)") ;
        }

        OK (GrB_Matrix_new (&A, gb_mxstring_to_type (pargin [2]),
            (int64_t) mxGetScalar (pargin [0]) , 
            (int64_t) mxGetScalar (pargin [1]))) ;
    }

    //--------------------------------------------------------------------------
    // export the output matrix A back to MATLAB
    //--------------------------------------------------------------------------

    pargout [0] = gb_export_to_mxstruct (&A) ;
}
