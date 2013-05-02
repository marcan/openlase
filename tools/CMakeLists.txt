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

include_directories (${CMAKE_SOURCE_DIR}/include)
link_directories (${CMAKE_BINARY_DIR}/libol)

add_executable(playilda playilda.c)
target_link_libraries(playilda ${JACK_LIBRARIES})

if(FFMPEG_FOUND AND BUILD_TRACER)
  include_directories(${FFMPEG_INCLUDE_DIR})
  add_executable(playvid playvid.c)
  target_link_libraries(playvid openlase ${FFMPEG_LIBRARIES} avresample)
else()
  message(STATUS "Will NOT build playvid (FFmpeg or tracer missing)")
endif()

if(OPENGL_FOUND AND GLUT_FOUND)
  add_executable(simulator simulator.c)
  include_directories(${OPENGL_INCLUDE_DIRS} ${GLUT_INCLUDE_DIRS})
  target_link_libraries(simulator m ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} ${JACK_LIBRARIES})
else()
  message(STATUS "Will NOT build simulator (OpenGL or GLUT missing)")
endif()

add_subdirectory(qplayvid)
