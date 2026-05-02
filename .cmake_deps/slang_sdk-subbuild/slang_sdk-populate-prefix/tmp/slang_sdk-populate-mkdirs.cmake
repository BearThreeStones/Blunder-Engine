# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "E:/Dev/Blunder-Engine/.cmake_deps/slang_sdk-src"
  "E:/Dev/Blunder-Engine/.cmake_deps/slang_sdk-build"
  "E:/Dev/Blunder-Engine/.cmake_deps/slang_sdk-subbuild/slang_sdk-populate-prefix"
  "E:/Dev/Blunder-Engine/.cmake_deps/slang_sdk-subbuild/slang_sdk-populate-prefix/tmp"
  "E:/Dev/Blunder-Engine/.cmake_deps/slang_sdk-subbuild/slang_sdk-populate-prefix/src/slang_sdk-populate-stamp"
  "E:/Dev/Blunder-Engine/.cmake_deps/slang_sdk-subbuild/slang_sdk-populate-prefix/src"
  "E:/Dev/Blunder-Engine/.cmake_deps/slang_sdk-subbuild/slang_sdk-populate-prefix/src/slang_sdk-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "E:/Dev/Blunder-Engine/.cmake_deps/slang_sdk-subbuild/slang_sdk-populate-prefix/src/slang_sdk-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "E:/Dev/Blunder-Engine/.cmake_deps/slang_sdk-subbuild/slang_sdk-populate-prefix/src/slang_sdk-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
