# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/ark30/Files/PPN/FineTrading/build/_deps/curl-src")
  file(MAKE_DIRECTORY "/home/ark30/Files/PPN/FineTrading/build/_deps/curl-src")
endif()
file(MAKE_DIRECTORY
  "/home/ark30/Files/PPN/FineTrading/build/_deps/curl-build"
  "/home/ark30/Files/PPN/FineTrading/build/_deps/curl-subbuild/curl-populate-prefix"
  "/home/ark30/Files/PPN/FineTrading/build/_deps/curl-subbuild/curl-populate-prefix/tmp"
  "/home/ark30/Files/PPN/FineTrading/build/_deps/curl-subbuild/curl-populate-prefix/src/curl-populate-stamp"
  "/home/ark30/Files/PPN/FineTrading/build/_deps/curl-subbuild/curl-populate-prefix/src"
  "/home/ark30/Files/PPN/FineTrading/build/_deps/curl-subbuild/curl-populate-prefix/src/curl-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/ark30/Files/PPN/FineTrading/build/_deps/curl-subbuild/curl-populate-prefix/src/curl-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/ark30/Files/PPN/FineTrading/build/_deps/curl-subbuild/curl-populate-prefix/src/curl-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
