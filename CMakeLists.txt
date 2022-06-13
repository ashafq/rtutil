# Copyright (C) 2022 Ayan Shafqat <ayan.x.shafqat@gmail.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 3 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required (VERSION 3.12)

# Project name and version
project (rtplay VERSION 1.0
         LANGUAGES CXX)

# Compiler options
set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED True)

option (ENABLE_LINT "Enable static code analysis" OFF)

if((CMAKE_CXX_COMPILER_ID STREQUAL "GNU") OR
   (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang"))
  add_compile_options(-Wall -Wextra -ggdb3)

  if (ENABLE_LINT)
    set(SANITIZE_FLAGS "-fsanitize=address,pointer-compare,undefined")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZE_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZE_FLAGS}")
    set(CMAKE_CXX_CLANG_TIDY
        clang-tidy;
        -header-filter=.,
        -checks=-*,cppcoreguidelines-*,-cppcoreguidelines-avoid-magic-numbers,-cppcoreguidelines-pro-bounds-constant-array-index)
  endif ()
endif ()

# Find external dependencies
list (APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
find_package (cxxopts REQUIRED)
find_package (tabulate REQUIRED)
find_package (SndFile REQUIRED)

# There are no conan package of rtaudio, so need to fetch
if (WIN32)
  include(FetchContent)
  FetchContent_Declare (
      rtaudio
      GIT_REPOSITORY https://github.com/thestk/rtaudio.git
      GIT_TAG        46b01b5b134f33d8ddc3dab76829d4b1350e0522 # release-5.2.0
      )
  FetchContent_MakeAvailable (rtaudio)
  set (RTAUDIO_INCLUDE_DIRS ${rtaudio_SOURCE_DIRS})
  set (RTAUDIO_LIBRARIES rtaudio)
else ()
  find_package (PkgConfig)
  pkg_check_modules (RTAUDIO REQUIRED rtaudio)
endif ()

set (PROJECT_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${cxxopts_INCLUDE_DIRS}
    ${SndFile_INCLUDE_DIRS}
    ${tabulate_INCLUDE_DIRS}
    ${RTAUDIO_INCLUDE_DIRS})

set (PROJECT_LIBRARIES
    ${cxxopts_LIBRARIES}
    ${SndFile_LIBRARIES}
    ${tabulate_LIBRARIES}
    ${RTAUDIO_LIBRARIES})

add_subdirectory (${CMAKE_SOURCE_DIR}/source)
