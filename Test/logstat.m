function logstat (testscript, threads, jit_controls)
%LOGSTAT run a GraphBLAS test and log the results to log.txt 
%
% logstat (testscript, threads, jit_control)
%
% threads: defaults to threads{1} = [4 1], which uses 4 threads and a tiny
% chunk size of 1.
%
% jit_control: a parameter for GB_mex_jit_control (-1: reset, 0 to 5: off,
% pause, run, load, on).  Defaults to -1, so the JIT kernels from the prior
% test are cleared from the JIT hash table, and then the JIT is renabled.  This
% is to prevent a sequence of many tests to run out of memory from loading too
% many JIT kernels.  If jit_control is empty, the JIT control is left
% unchanged.

% SuiteSparse:GraphBLAS, Timothy A. Davis, (c) 2017-2023, All Rights Reserved.
% SPDX-License-Identifier: Apache-2.0

GB_mex_finalize ;
[debug, compact, malloc, covered] = GB_mex_debug ;

% default JIT controls
if (nargin < 3)
    jit_controls {1} = -4 ;     % reset
    jit_controls {2} = 0 ;      % off
end

if (nargin < 2)
    % by default, use 4 threads and a tiny chunk size of 1
    threads {1} = [4 1] ;
else
    % only the # of threads is specified; also set the chunk size to 1
    if (isscalar (threads) && isnumeric (threads))
        threads = max (threads, 1) ;
        t {1} = [threads 1] ;
        threads = t ;
    end
end

n = 1 ;
if (~isempty (strfind (pwd, 'Tcov')))
    % load in the # of lines in the test coverage
    fp = fopen ('tmp_cover/count', 'r') ;
    n = textscan (fp, '%f') ;
    n = n {1} ;
    fclose (fp) ;
end

for jit_trial = 1:length (jit_controls)

    jit_control = jit_controls {jit_trial} ;

    fprintf ('jit: %d\n', jit_control) ;
    if (jit_control < 0)
        GB_mex_jit_control (abs (jit_control)) ;
    elseif (~isempty (jit_control))
        GB_mex_jit_control (jit_control) ;
    end

    for trial = 1:length (threads)

        clast = grb_get_coverage ;

        nthreads_and_chunk = threads {trial} ;
        nthreads = nthreads_and_chunk (1) ;
        chunk    = nthreads_and_chunk (2) ;
        nthreads_set (nthreads, chunk) ;

        if (nargin == 0)
            f = fopen ('log.txt', 'a') ;
            fprintf (f, '\n----------------------------------------------') ;
            if (debug)
                fprintf (f, ' [debug]') ;
            end
            if (compact)
                fprintf (f, ' [compact]') ;
            end
            if (malloc)
                fprintf (f, ' [malloc]') ;
            end
            if (covered)
                fprintf (f, ' [cover]') ;
            end
            fprintf (f, '\n') ;
            fclose (f) ;
            return
        end

        fprintf ('\n======== test: %-10s ', testscript) ;

        if (debug)
            fprintf (' [debug]') ;
        end
        if (compact)
            fprintf (' [compact]') ;
        end
        if (malloc)
            fprintf (' [malloc]') ;
        end
        if (covered)
            fprintf (' [cover]') ;
        end
        fprintf (' [nthreads: %d chunk: %g]', nthreads, chunk) ;
        fprintf (' jit: %d\n', GB_mex_jit_control) ;

        t1 = tic ;
        runtest (testscript)
        t = toc (t1) ;

        f = fopen ('log.txt', 'a') ;

        s = datestr (now) ;

        % trim the year from the date
        s = s ([1:6 12:end]) ;

        fprintf (   '%s %-11s %7.1f sec ', s, testscript, t) ;
        fprintf (f, '%s %-11s %7.1f sec ', s, testscript, t) ;

        if (~isempty (strfind (pwd, 'Tcov')))
            global GraphBLAS_debug GraphBLAS_grbcov
            save grbstat GraphBLAS_debug GraphBLAS_grbcov testscript
            if (isempty (GraphBLAS_debug))
                GraphBLAS_debug = false ;
            end
            if (~isempty (GraphBLAS_grbcov))
                c = sum (GraphBLAS_grbcov > 0) ;
                if (c == n)
                    % full coverage reached with this test
                    fprintf (   '%5d:   all %5d full 100%% %8.2f/s', ...
                        c - clast, n, (c-clast) / t) ;
                    fprintf (f, '%5d:   all %5d full 100%% %8.2f/s', ...
                        c - clast, n, (c-clast) / t) ;
                elseif (c == clast)
                    % no new coverage at all with this test
                    fprintf (   '     : %5d of %5d %5.1f%%', ...
                        n-c, n, 100 * (c/n)) ;
                    fprintf (f, '     : %5d of %5d %5.1f%%', ...
                        n-c, n, 100 * (c/n)) ;
                else
                    fprintf (   '%5d: %5d of %5d %5.1f%% %8.2f/s', ...
                        c - clast, n-c, n, 100 * (c/n), (c-clast) / t) ;
                    fprintf (f, '%5d: %5d of %5d %5.1f%% %8.2f/s', ...
                        c - clast, n-c, n, 100 * (c/n), (c-clast) / t) ;
                end
                if (debug)
                    fprintf (' [debug]') ;
                end
                if (compact)
                    fprintf (' [compact]') ;
                end
                if (malloc)
                    fprintf (' [malloc]') ;
                end
                if (covered)
                    fprintf (' [cover]') ;
                end
            end
        end

        fprintf (   '\n') ;
        fprintf (f, '\n') ;
        fclose (f) ;
    end
end

