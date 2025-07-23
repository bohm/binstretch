
function(build_test_recursion BINS R S MONOT SCALE)
    add_executable(explore-rec-${BINS}-${R}-${S}-${MONOT}-${SCALE} tests/explore.cpp
            search/cache/guar64.hpp
            search/cache/guar_locks.hpp
            search/cache/guarantee.hpp
            search/cache/loadconf.hpp
            search/cache/state.hpp
            search/dag/basics.hpp
            search/dag/class.hpp
            search/dag/cloning.hpp
            search/dag/consistency.hpp
            search/dag/dag.hpp
            search/dag/partial.hpp
            search/dag/print.hpp
            search/dynprog/algo.hpp
            search/dynprog/wrappers.hpp
            search/minibs/binary_storage.hpp
            search/minibs/feasibility.hpp
            search/itemconf.hpp
            search/minibs/minibs.hpp
            search/minibs/minidp.hpp
            search/minimax/auxiliary.hpp
            search/minimax/computation.hpp
            search/minimax/recursion.hpp
            search/minimax/stack_minimax.hpp
            search/minimax/sequencing.hpp
            search/net/local/broadcaster.hpp
            search/net/local/comm_basics.hpp
            search/net/local/local_communicator.hpp
            search/net/local/message_arrays.hpp
            search/net/local/synchronizer.hpp
            search/net/local/threadsafe_printer.hpp
            search/net/batches.hpp
            search/poset/poset.hpp
            search/strategies/abstract.hpp
            search/strategies/basic.hpp
            search/strategies/heuristical.hpp
            search/strategies/insight.hpp
            search/strategies/insight_methods.hpp
            search/advisor.hpp
            search/assumptions.hpp
            search/binconf.hpp
            search/cleanup.hpp
            search/common.hpp
            search/constants.hpp
            search/dfs.hpp
            search/exceptions.hpp
            search/filetools.hpp
            search/fits.hpp
            search/functions.hpp
            search/gs.hpp
            search/hash.hpp
            search/heur_adv.hpp
            search/heur_alg_knownsum.hpp
            search/heur_classes.hpp
            search/layers.hpp
            search/loadfile.hpp
            search/maxfeas.hpp
            search/measure_structures.hpp
            search/optconf.hpp
            search/overseer.hpp
            search/overseer_methods.hpp
            search/performance_timer.hpp
            search/positional.hpp
            search/queen.hpp
            search/queen_methods.hpp
            search/sapling_manager.hpp
            search/saplings.hpp
            search/savefile.hpp
            search/server_properties.hpp
            search/small_classes.hpp
            search/strategy.hpp
            search/tasks/tasks.hpp
            search/thread_attr.hpp
            search/updater.hpp
            search/worker.hpp
            search/worker_methods.hpp
            search/minimax/heuristic_visits.hpp
            search/minimax/descend_ascend.hpp
            search/loadconf.hpp
            search/minibs/minibs-three.hpp
            search/minibs/midgame_feasibility.hpp
            search/minibs/fingerprint_storage.hpp
            search/minibs/fingerprints.hpp
            search/binomial_index.hpp
            search/minibs/knownsum_game.hpp
            search/minibs/flat_data.hpp)

    add_dependencies(tests explore-rec-${BINS}-${R}-${S}-${MONOT}-${SCALE})
    set_target_properties(explore-rec-${BINS}-${R}-${S}-${MONOT}-${SCALE}
            PROPERTIES
            OUTPUT_NAME explore-rec-${SCALE}-mon-${MONOT}
    )


    target_compile_definitions(explore-rec-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IBINS=${BINS})
    target_compile_definitions(explore-rec-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IR=${R})
    target_compile_definitions(explore-rec-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IS=${S})
    target_compile_definitions(explore-rec-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC ISCALE=${SCALE})
    target_compile_definitions(explore-rec-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IMONOT=${MONOT})
    target_compile_definitions(explore-rec-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IRECURSION=1) # Recursion.
    target_compile_definitions(explore-rec-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IPACKED=0) # Recursion.


endfunction()

function(build_test_stack BINS R S MONOT SCALE)
    add_executable(explore-stack-${BINS}-${R}-${S}-${MONOT}-${SCALE} tests/explore.cpp)
    add_dependencies(tests explore-stack-${BINS}-${R}-${S}-${MONOT}-${SCALE})
    set_target_properties(explore-stack-${BINS}-${R}-${S}-${MONOT}-${SCALE}
            PROPERTIES
            OUTPUT_NAME explore-stack-${SCALE}-mon-${MONOT}
    )
    target_compile_definitions(explore-stack-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IBINS=${BINS})
    target_compile_definitions(explore-stack-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IR=${R})
    target_compile_definitions(explore-stack-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IS=${S})
    target_compile_definitions(explore-stack-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC ISCALE=${SCALE})
    target_compile_definitions(explore-stack-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IMONOT=${MONOT})
    target_compile_definitions(explore-stack-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IRECURSION=0) # Stack.
    target_compile_definitions(explore-stack-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IPACKED=0) # Stack.
endfunction()

function(build_test_packed BINS R S MONOT SCALE)
    add_executable(explore-packed-${BINS}-${R}-${S}-${MONOT}-${SCALE} tests/explore.cpp)
    add_dependencies(tests explore-packed-${BINS}-${R}-${S}-${MONOT}-${SCALE})
    set_target_properties(explore-packed-${BINS}-${R}-${S}-${MONOT}-${SCALE}
            PROPERTIES
            OUTPUT_NAME explore-packed-${SCALE}-mon-${MONOT}
    )
    target_compile_definitions(explore-packed-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IBINS=${BINS})
    target_compile_definitions(explore-packed-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IR=${R})
    target_compile_definitions(explore-packed-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IS=${S})
    target_compile_definitions(explore-packed-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC ISCALE=${SCALE})
    target_compile_definitions(explore-packed-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IMONOT=${MONOT})
    target_compile_definitions(explore-packed-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IRECURSION=0) # Stack.
    target_compile_definitions(explore-packed-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IPACKED=1) # Stack.
endfunction()

function(build_knownsum_tests)
    add_executable(knownsum-game-tests tests/knownsum-game-tests.cpp)
    add_dependencies(tests knownsum-game-tests)
    target_compile_definitions(knownsum-game-tests PUBLIC IBINS=12)
    target_compile_definitions(knownsum-game-tests PUBLIC IR=19)
    target_compile_definitions(knownsum-game-tests PUBLIC IS=14)
    target_compile_definitions(knownsum-game-tests PUBLIC ISCALE=3)
    target_compile_definitions(knownsum-game-tests PUBLIC IMONOT=1)
    target_compile_definitions(knownsum-game-tests PUBLIC IRECURSION=0) # Stack.
    target_compile_definitions(knownsum-game-tests PUBLIC IPACKED=1) # Stack.
endfunction()

function(build_minibs_performance BINS R S MONOT SCALE)
    add_executable(minibs-performance-${BINS}-${R}-${S}-${MONOT}-${SCALE} tests/minibs-performance.cpp)
    add_dependencies(tests minibs-performance-${BINS}-${R}-${S}-${MONOT}-${SCALE})
    set_target_properties(minibs-performance-${BINS}-${R}-${S}-${MONOT}-${SCALE}
            PROPERTIES
            OUTPUT_NAME minibs-performance-${SCALE}-mon-${MONOT}
    )
    target_compile_definitions(minibs-performance-${BINS}-${R}-${S}-${MONOT}-${SCALE}
            PUBLIC IBINS=${BINS} IR=${R} IS=${S} ISCALE=${SCALE} IMONOT=${MONOT} IRECURSION=0 IPACKED=0)
endfunction()

function(build_minibs_stats BINS R S MONOT SCALE)
    add_executable(minibs-stats-${BINS}-${R}-${S}-${MONOT}-${SCALE} tests/minibs-stats.cpp)
    add_dependencies(tests minibs-stats-${BINS}-${R}-${S}-${MONOT}-${SCALE})
    set_target_properties(minibs-stats-${BINS}-${R}-${S}-${MONOT}-${SCALE}
            PROPERTIES
            OUTPUT_NAME minibs-stats-${SCALE}-mon-${MONOT}
    )
    target_compile_definitions(minibs-stats-${BINS}-${R}-${S}-${MONOT}-${SCALE}
            PUBLIC IBINS=${BINS} IR=${R} IS=${S} ISCALE=${SCALE} IMONOT=${MONOT} IRECURSION=0 IPACKED=0)
endfunction()


function(build_tests BINS R S MONOT SCALE)
    if (${MONOT} EQUAL -1)
        recommend_monotonicity(${BINS} ${R} ${S})
        message("Recommending monotonicity ${MONOT}.")
    else ()
        message("Pre-set monotonicity ${MONOT}.")
    endif ()

    if (${SCALE} EQUAL -1)
        recommend_scaling(${BINS} ${R} ${S})
        message("Recommending scaling factor ${SCALE}.")
    else ()
        message("Pre-set scaling factor ${SCALE}.")
    endif ()

    message("Building search tests for ${BINS} bins, ratio ${R}/${S}, monotonicity ${MONOT}, minibs scaling ${SCALE}.")

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "./${BINS}-${R}-${S}/")

    # build_test_packed(${BINS} ${R} ${S} ${MONOT} ${SCALE})
    # build_test_recursion(${BINS} ${R} ${S} ${MONOT} ${SCALE})
    # build_test_stack(${BINS} ${R} ${S} ${MONOT} ${SCALE})
    build_minibs_performance(${BINS} ${R} ${S} ${MONOT} ${SCALE})
    build_minibs_stats(${BINS} ${R} ${S} ${MONOT} ${SCALE})
endfunction()
