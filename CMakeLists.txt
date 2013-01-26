#         OpenLase - a realtime laser graphics toolkit
#
# Copyright (C) 2009-2011 Hector Martin "marcan" <hector@marcansoft.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 or version 3.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#

project(openlase)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/Modules/")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(BITS 64)
else()
  set(BITS 32)
endif()

find_package(Threads REQUIRED)
find_package(JACK REQUIRED)
find_package(PythonInterp REQUIRED)
find_package(Qt4)
find_package(FFmpeg)
find_package(Swscale)
find_package(OpenGL)
find_package(GLUT)
find_package(ALSA)
find_package(Curses)

if(CMAKE_C_FLAGS STREQUAL "")
  set(CMAKE_C_FLAGS "-O3 -g")
endif()

add_definitions(-Wall)

if(NOT DEFINED BUILD_TRACER)
  set(BUILD_TRACER "Y" CACHE BOOL "Build the image tracer (x86 only)" FORCE)
endif()

add_subdirectory (libol)
add_subdirectory (output)
add_subdirectory (tools)
add_subdirectory (python)
add_subdirectory (examples)
