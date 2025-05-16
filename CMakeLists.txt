cmake_minimum_required(VERSION 3.26)
project(clion)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

message("CMake build type: ${CMAKE_BUILD_TYPE}.")

add_custom_target(search)
add_custom_target(tests)
include(recommend.cmake)
include(build_tests.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g2 -march=native -O2")
# set(CMAKE_CXX_FLAGS_RELEASE "-march=native -Ofast -march=native -fwhole-program -fno-exceptions -fno-rtti -finline-functions")
set(CMAKE_CXX_FLAGS_RELEASE "-g -O2") # VTune recommendation, use for profiling.

if (NOT CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DNDEBUG)
endif()

include_directories(search)
include_directories(search/cache)
include_directories(search/dag)
include_directories(search/dynprog)
include_directories(search/minibs)
include_directories(search/minimax)
include_directories(search/net)
include_directories(search/net/local)
include_directories(search/net/mpi)
include_directories(search/poset)
include_directories(search/strategies)
include_directories(search/weights)
include_directories(../parallel-hashmap)


function(build_search BINS R S MONOT SCALE)
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

    message("Building lower bound search for ${BINS} bins, ratio ${R}/${S}, monotonicity ${MONOT}, minibs scaling ${SCALE}.")


    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "./${BINS}-${R}-${S}/")

    add_executable(search-${BINS}-${R}-${S}-${MONOT}-${SCALE} search/main.cpp
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

    add_dependencies(search search-${BINS}-${R}-${S}-${MONOT}-${SCALE})
    set_target_properties(search-${BINS}-${R}-${S}-${MONOT}-${SCALE}
            PROPERTIES
            OUTPUT_NAME search-${SCALE}
    )

    target_compile_definitions(search-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IBINS=${BINS})
    target_compile_definitions(search-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IR=${R})
    target_compile_definitions(search-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IS=${S})
    target_compile_definitions(search-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC ISCALE=${SCALE})
    target_compile_definitions(search-${BINS}-${R}-${S}-${MONOT}-${SCALE} PUBLIC IMONOT=${MONOT})

endfunction()

# build_search(3 4 3 -1 -1) # Lower bound.
# build_search(3 6 4 -1 -1) # Not a lower bound.
# build_search(3 19 14 -1 -1) # Lower bound.
# build_search(3 56 41 -1 -1) # Not a lower bound.
# build_search(3 164 120 -1 -1) # Lower bound.
# build_search(3 149 109 -1 -1)
# build_search(8 19 14 -1 -1)
# build_search(4 19 14 -1 -1)
build_tests(4 19 14 -1 -1)
build_tests(7 19 14 -1 -1)
# build_tests(3 19 14 -1 -1)


#add_executable(minibs-stats minitools/minibs-stats.cpp)
#
#set_target_properties(minibs-stats
#        PROPERTIES
#        OUTPUT_NAME minibs-stats-${ISCALE}
#)
#
#add_executable(listsaplings minitools/listsaplings.cpp)
#
#
#set_target_properties(listsaplings
#        PROPERTIES
#        OUTPUT_NAME listsaplings-${ISCALE}
#)
#
#add_executable(awt minitools/alg-winning-table.cpp
#        search/cache/guar64.hpp
#        search/cache/guar_locks.hpp
#        search/cache/guarantee.hpp
#        search/cache/loadconf.hpp
#        search/cache/state.hpp
#        search/dag/basics.hpp
#        search/dag/class.hpp
#        search/dag/cloning.hpp
#        search/dag/consistency.hpp
#        search/dag/dag.hpp
#        search/dag/partial.hpp
#        search/dag/print.hpp
#        search/dynprog/algo.hpp
#        search/dynprog/wrappers.hpp
#        search/minibs/binary_storage.hpp
#        search/minibs/feasibility.hpp
#        search/itemconf.hpp
#        search/minibs/minibs.hpp
#        search/minibs/minidp.hpp
#        search/minimax/auxiliary.hpp
#        search/minimax/computation.hpp
#        search/minimax/recursion.hpp
#        search/minimax/sequencing.hpp
#        search/net/local/broadcaster.hpp
#        search/net/local/comm_basics.hpp
#        search/net/local/local_communicator.hpp
#        search/net/local/message_arrays.hpp
#        search/net/local/synchronizer.hpp
#        search/net/local/threadsafe_printer.hpp
#        search/net/batches.hpp
#        search/poset/poset.hpp
#        search/strategies/abstract.hpp
#        search/strategies/basic.hpp
#        search/strategies/heuristical.hpp
#        search/strategies/insight.hpp
#        search/strategies/insight_methods.hpp
#        search/advisor.hpp
#        search/assumptions.hpp
#        search/binconf.hpp
#        search/cleanup.hpp
#        search/common.hpp
#        search/constants.hpp
#        search/dfs.hpp
#        search/exceptions.hpp
#        search/filetools.hpp
#        search/fits.hpp
#        search/functions.hpp
#        search/gs.hpp
#        search/hash.hpp
#        search/heur_adv.hpp
#        search/heur_alg_knownsum.hpp
#        search/heur_classes.hpp
#        search/layers.hpp
#        search/loadfile.hpp
#        search/maxfeas.hpp
#        search/measure_structures.hpp
#        search/optconf.hpp
#        search/overseer.hpp
#        search/overseer_methods.hpp
#        search/performance_timer.hpp
#        search/positional.hpp
#        search/queen.hpp
#        search/queen_methods.hpp
#        search/sapling_manager.hpp
#        search/saplings.hpp
#        search/savefile.hpp
#        search/server_properties.hpp
#        search/small_classes.hpp
#        search/strategy.hpp
#        search/tasks/tasks.hpp
#        search/thread_attr.hpp
#        search/updater.hpp
#        search/worker.hpp
#        search/worker_methods.hpp
#        search/minimax/heuristic_visits.hpp
#        search/minimax/descend_ascend.hpp
#        search/loadconf.hpp
#        search/minibs/minibs-three.hpp
#        search/minibs/midgame_feasibility.hpp
#        search/binomial_index.hpp
#        search/minibs/knownsum_game.hpp
#        search/minibs/flat_data.hpp)
#
#set_target_properties(awt
#        PROPERTIES
#        OUTPUT_NAME awt-${ISCALE}
#)
#
#add_executable(all-losing minitools/all-losing.cpp
#        search/cache/guar64.hpp
#        search/cache/guar_locks.hpp
#        search/cache/guarantee.hpp
#        search/cache/loadconf.hpp
#        search/cache/state.hpp
#        search/dag/basics.hpp
#        search/dag/class.hpp
#        search/dag/cloning.hpp
#        search/dag/consistency.hpp
#        search/dag/dag.hpp
#        search/dag/partial.hpp
#        search/dag/print.hpp
#        search/dynprog/algo.hpp
#        search/dynprog/wrappers.hpp
#        search/minibs/binary_storage.hpp
#        search/minibs/feasibility.hpp
#        search/itemconf.hpp
#        search/minibs/minibs.hpp
#        search/minibs/minidp.hpp
#        search/minimax/auxiliary.hpp
#        search/minimax/computation.hpp
#        search/minimax/recursion.hpp
#        search/minimax/sequencing.hpp
#        search/net/local/broadcaster.hpp
#        search/net/local/comm_basics.hpp
#        search/net/local/local_communicator.hpp
#        search/net/local/message_arrays.hpp
#        search/net/local/synchronizer.hpp
#        search/net/local/threadsafe_printer.hpp
#        search/net/batches.hpp
#        search/poset/poset.hpp
#        search/strategies/abstract.hpp
#        search/strategies/basic.hpp
#        search/strategies/heuristical.hpp
#        search/strategies/insight.hpp
#        search/strategies/insight_methods.hpp
#        search/advisor.hpp
#        search/assumptions.hpp
#        search/binconf.hpp
#        search/cleanup.hpp
#        search/common.hpp
#        search/constants.hpp
#        search/dfs.hpp
#        search/exceptions.hpp
#        search/filetools.hpp
#        search/fits.hpp
#        search/functions.hpp
#        search/gs.hpp
#        search/hash.hpp
#        search/heur_adv.hpp
#        search/heur_alg_knownsum.hpp
#        search/heur_classes.hpp
#        search/layers.hpp
#        search/loadfile.hpp
#        search/maxfeas.hpp
#        search/measure_structures.hpp
#        search/optconf.hpp
#        search/overseer.hpp
#        search/overseer_methods.hpp
#        search/performance_timer.hpp
#        search/positional.hpp
#        search/queen.hpp
#        search/queen_methods.hpp
#        search/sapling_manager.hpp
#        search/saplings.hpp
#        search/savefile.hpp
#        search/server_properties.hpp
#        search/small_classes.hpp
#        search/strategy.hpp
#        search/tasks/tasks.hpp
#        search/thread_attr.hpp
#        search/updater.hpp
#        search/worker.hpp
#        search/worker_methods.hpp
#        search/minimax/heuristic_visits.hpp
#        search/minimax/descend_ascend.hpp
#        search/loadconf.hpp
#        search/minibs/minibs-three.hpp
#        search/minibs/midgame_feasibility.hpp
#        search/binomial_index.hpp
#        search/minibs/knownsum_game.hpp
#        search/minibs/flat_data.hpp)
#
#set_target_properties(all-losing
#        PROPERTIES
#        OUTPUT_NAME all-losing-${ISCALE}
#)
#
#add_executable(sand-on-ab minitools/sand-on-ab.cpp
#        search/cache/guar64.hpp
#        search/cache/guar_locks.hpp
#        search/cache/guarantee.hpp
#        search/cache/loadconf.hpp
#        search/cache/state.hpp
#        search/dag/basics.hpp
#        search/dag/class.hpp
#        search/dag/cloning.hpp
#        search/dag/consistency.hpp
#        search/dag/dag.hpp
#        search/dag/partial.hpp
#        search/dag/print.hpp
#        search/dynprog/algo.hpp
#        search/dynprog/wrappers.hpp
#        search/minibs/binary_storage.hpp
#        search/minibs/feasibility.hpp
#        search/itemconf.hpp
#        search/minibs/minibs.hpp
#        search/minibs/minidp.hpp
#        search/minimax/auxiliary.hpp
#        search/minimax/computation.hpp
#        search/minimax/recursion.hpp
#        search/minimax/sequencing.hpp
#        search/net/local/broadcaster.hpp
#        search/net/local/comm_basics.hpp
#        search/net/local/local_communicator.hpp
#        search/net/local/message_arrays.hpp
#        search/net/local/synchronizer.hpp
#        search/net/local/threadsafe_printer.hpp
#        search/net/batches.hpp
#        search/poset/poset.hpp
#        search/strategies/abstract.hpp
#        search/strategies/basic.hpp
#        search/strategies/heuristical.hpp
#        search/strategies/insight.hpp
#        search/strategies/insight_methods.hpp
#        search/advisor.hpp
#        search/assumptions.hpp
#        search/binconf.hpp
#        search/cleanup.hpp
#        search/common.hpp
#        search/constants.hpp
#        search/dfs.hpp
#        search/exceptions.hpp
#        search/filetools.hpp
#        search/fits.hpp
#        search/functions.hpp
#        search/gs.hpp
#        search/hash.hpp
#        search/heur_adv.hpp
#        search/heur_alg_knownsum.hpp
#        search/heur_classes.hpp
#        search/layers.hpp
#        search/loadfile.hpp
#        search/maxfeas.hpp
#        search/measure_structures.hpp
#        search/optconf.hpp
#        search/overseer.hpp
#        search/overseer_methods.hpp
#        search/performance_timer.hpp
#        search/positional.hpp
#        search/queen.hpp
#        search/queen_methods.hpp
#        search/sapling_manager.hpp
#        search/saplings.hpp
#        search/savefile.hpp
#        search/server_properties.hpp
#        search/small_classes.hpp
#        search/strategy.hpp
#        search/tasks/tasks.hpp
#        search/thread_attr.hpp
#        search/updater.hpp
#        search/worker.hpp
#        search/worker_methods.hpp
#        search/minimax/heuristic_visits.hpp
#        search/minimax/descend_ascend.hpp
#        search/loadconf.hpp
#        search/minibs/minibs-three.hpp
#        search/minibs/midgame_feasibility.hpp
#        search/binomial_index.hpp
#        search/minibs/knownsum_game.hpp
#        search/minibs/flat_data.hpp)
#
#set_target_properties(sand-on-ab
#        PROPERTIES
#        OUTPUT_NAME sand-on-ab-${ISCALE}
#)
#
#add_executable(painter painter/painter.cpp
#        search/cache/guar64.hpp
#        search/cache/guar_locks.hpp
#        search/cache/guarantee.hpp
#        search/cache/loadconf.hpp
#        search/cache/state.hpp
#        search/dag/basics.hpp
#        search/dag/class.hpp
#        search/dag/cloning.hpp
#        search/dag/consistency.hpp
#        search/dag/dag.hpp
#        search/dag/partial.hpp
#        search/dag/print.hpp
#        search/dynprog/algo.hpp
#        search/dynprog/wrappers.hpp
#        search/minibs/binary_storage.hpp
#        search/minibs/feasibility.hpp
#        search/itemconf.hpp
#        search/minibs/minibs.hpp
#        search/minibs/minidp.hpp
#        search/minimax/auxiliary.hpp
#        search/minimax/computation.hpp
#        search/minimax/recursion.hpp
#        search/minimax/sequencing.hpp
#        search/net/local/broadcaster.hpp
#        search/net/local/comm_basics.hpp
#        search/net/local/local_communicator.hpp
#        search/net/local/message_arrays.hpp
#        search/net/local/synchronizer.hpp
#        search/net/local/threadsafe_printer.hpp
#        search/net/batches.hpp
#        search/poset/poset.hpp
#        search/strategies/abstract.hpp
#        search/strategies/basic.hpp
#        search/strategies/heuristical.hpp
#        search/strategies/insight.hpp
#        search/strategies/insight_methods.hpp
#        search/advisor.hpp
#        search/assumptions.hpp
#        search/binconf.hpp
#        search/cleanup.hpp
#        search/common.hpp
#        search/constants.hpp
#        search/dfs.hpp
#        search/exceptions.hpp
#        search/filetools.hpp
#        search/fits.hpp
#        search/functions.hpp
#        search/gs.hpp
#        search/hash.hpp
#        search/heur_adv.hpp
#        search/heur_alg_knownsum.hpp
#        search/heur_classes.hpp
#        search/layers.hpp
#        search/loadfile.hpp
#        search/maxfeas.hpp
#        search/measure_structures.hpp
#        search/optconf.hpp
#        search/overseer.hpp
#        search/overseer_methods.hpp
#        search/performance_timer.hpp
#        search/positional.hpp
#        search/queen.hpp
#        search/queen_methods.hpp
#        search/sapling_manager.hpp
#        search/saplings.hpp
#        search/savefile.hpp
#        search/server_properties.hpp
#        search/small_classes.hpp
#        search/strategy.hpp
#        search/tasks/tasks.hpp
#        search/thread_attr.hpp
#        search/updater.hpp
#        search/worker.hpp
#        search/worker_methods.hpp
#        search/minimax/heuristic_visits.hpp
#        search/minimax/descend_ascend.hpp
#        search/loadconf.hpp
#        search/minibs/minibs-three.hpp
#        search/minibs/midgame_feasibility.hpp)
#
## Purge the two variables from the cache, so they are recomputed.
#unset(IMONOT)
#unset(ISCALE)
