# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "D:/Dev/Blunder-Engine/.cmake_deps/slang_sdk-src")
  file(MAKE_DIRECTORY "D:/Dev/Blunder-Engine/.cmake_deps/slang_sdk-src")
endif()
file(MAKE_DIRECTORY
  "D:/Dev/Blunder-Engine/.cmake_deps/slang_sdk-build"
  "D:/Dev/Blunder-Engine/.cmake_deps/slang_sdk-subbuild/slang_sdk-populate-prefix"
  "D:/Dev/Blunder-Engine/.cmake_deps/slang_sdk-subbuild/slang_sdk-populate-prefix/tmp"
  "D:/Dev/Blunder-Engine/.cmake_deps/slang_sdk-subbuild/slang_sdk-populate-prefix/src/slang_sdk-populate-stamp"
  "D:/Dev/Blunder-Engine/.cmake_deps/slang_sdk-subbuild/slang_sdk-populate-prefix/src"
  "D:/Dev/Blunder-Engine/.cmake_deps/slang_sdk-subbuild/slang_sdk-populate-prefix/src/slang_sdk-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/Dev/Blunder-Engine/.cmake_deps/slang_sdk-subbuild/slang_sdk-populate-prefix/src/slang_sdk-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/Dev/Blunder-Engine/.cmake_deps/slang_sdk-subbuild/slang_sdk-populate-prefix/src/slang_sdk-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
