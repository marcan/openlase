#         OpenLase - a realtime laser graphics toolkit
#
# Copyright (C) 2009-2011 Hector Martin "marcan" <hector@marcansoft.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 2.1 or version 3.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#

if(Qt5Widgets_FOUND AND FFMPEG_FOUND AND SWSCALE_FOUND AND BUILD_TRACER)
  QT5_WRAP_CPP(qplayvid_MOCS qplayvid_gui.h)

  include_directories(${CMAKE_CURRENT_BINARY_DIR})

  add_executable(qplayvid qplayvid.c qplayvid_gui.cpp ${qplayvid_MOCS})
  target_link_libraries(qplayvid ol ${FFMPEG_LIBRARIES} ${SWSCALE_LIBRARIES} Qt5::Widgets avresample)
else()
  message(STATUS "Will NOT build qplayvid (Qt5 or FFmpeg or tracer missing)")
endif()
