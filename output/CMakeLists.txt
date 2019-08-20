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

if(Qt5Widgets_FOUND)
  QT5_WRAP_UI(output_UIS_H output_settings.ui)
  QT5_WRAP_CPP(output_MOCS output_settings.h)

  include_directories(${CMAKE_CURRENT_BINARY_DIR} ${JACK_INCLUDE_DIR})

  add_executable(output output.cpp output_settings.cpp ${output_UIS_H} ${output_MOCS})
  target_link_libraries(output ${JACK_LIBRARIES} Qt5::Widgets)
else()
  message(STATUS "Will NOT build output (Qt4 missing)")
endif()
