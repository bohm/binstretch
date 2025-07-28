# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/runner/work/binstretch/binstretch/cmake_build/_deps/parallel_hashmap-src")
  file(MAKE_DIRECTORY "/home/runner/work/binstretch/binstretch/cmake_build/_deps/parallel_hashmap-src")
endif()
file(MAKE_DIRECTORY
  "/home/runner/work/binstretch/binstretch/cmake_build/_deps/parallel_hashmap-build"
  "/home/runner/work/binstretch/binstretch/cmake_build/_deps/parallel_hashmap-subbuild/parallel_hashmap-populate-prefix"
  "/home/runner/work/binstretch/binstretch/cmake_build/_deps/parallel_hashmap-subbuild/parallel_hashmap-populate-prefix/tmp"
  "/home/runner/work/binstretch/binstretch/cmake_build/_deps/parallel_hashmap-subbuild/parallel_hashmap-populate-prefix/src/parallel_hashmap-populate-stamp"
  "/home/runner/work/binstretch/binstretch/cmake_build/_deps/parallel_hashmap-subbuild/parallel_hashmap-populate-prefix/src"
  "/home/runner/work/binstretch/binstretch/cmake_build/_deps/parallel_hashmap-subbuild/parallel_hashmap-populate-prefix/src/parallel_hashmap-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/runner/work/binstretch/binstretch/cmake_build/_deps/parallel_hashmap-subbuild/parallel_hashmap-populate-prefix/src/parallel_hashmap-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/runner/work/binstretch/binstretch/cmake_build/_deps/parallel_hashmap-subbuild/parallel_hashmap-populate-prefix/src/parallel_hashmap-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
