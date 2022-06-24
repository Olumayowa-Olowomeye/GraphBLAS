function [arg1, arg2] = bandwidth (G, uplo)
%BANDWIDTH matrix bandwidth.
% [lo, hi] = bandwidth (G) returns the upper and lower bandwidth of G.
% lo = bandwidth (G, 'lower') returns just the lower bandwidth.
% hi = bandwidth (G, 'upper') returns just the upper bandwidth.
%
% See also GrB/isbanded, GrB/isdiag, GrB/istril, GrB/istriu.

% SuiteSparse:GraphBLAS, Timothy A. Davis, (c) 2017-2022, All Rights Reserved.
% SPDX-License-Identifier: GPL-3.0-or-later

if (nargin == 1)
    % compute lo, and compute hi if present in output argument list
    [lo, hi] = gbbandwidth (G.opaque, 1, nargout > 1) ;
    arg1 = lo ;
    arg2 = hi ;
else
    if (nargout > 1)
        error ('too many output arguments') ;
    elseif isequal (uplo, 'lower')
        [lo, hi] = gbbandwidth (G.opaque, 1, 0) ;
        arg1 = lo ;
    elseif isequal (uplo, 'upper')
        [lo, hi] = gbbandwidth (G.opaque, 0, 1) ;
        arg1 = hi ;
    else
        error ('unrecognized option') ;
    end
end

