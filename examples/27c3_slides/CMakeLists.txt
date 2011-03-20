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

if(CURSES_FOUND AND FFMPEG_FOUND)
  include_directories(${CMAKE_SOURCE_DIR}/tools ${CURSES_INCLUDE_DIR})

  add_executable(slides
    main.c 3ddemos.c static.c metaballs.c circlescope.c avstream.c video.c pong.c

    ${CMAKE_CURRENT_BINARY_DIR}/openlase-logo.ild
    ${CMAKE_CURRENT_BINARY_DIR}/27c3-logo.ild
    ${CMAKE_CURRENT_BINARY_DIR}/jack-logo.ild
    ${CMAKE_CURRENT_BINARY_DIR}/output.ild
  )

  target_link_libraries(slides openlase ${FFMPEG_LIBRARIES} ${CURSES_LIBRARIES})

  function(svg2ild NAME)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.ild
      DEPENDS ${CMAKE_SOURCE_DIR}/tools/svg2ild.py
      MAIN_DEPENDENCY ${NAME}.svg
      DEPENDS ${NAME}.svg ${NAME}.cfg
      COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/svg2ild.py -q ${ARGN} -cfg ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}.cfg ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}.svg ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.ild)
  endfunction()

  svg2ild(openlase-logo -noopt)
  svg2ild(27c3-logo)
  svg2ild(jack-logo)
  svg2ild(output)
else()
  message(STATUS "Will NOT build 27c3_slides (curses or FFmpeg missing)")
endif()
