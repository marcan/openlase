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

add_executable(circlescope circlescope.c)
target_link_libraries(circlescope ${JACK_LIBRARIES} m)
install(TARGETS circlescope RUNTIME DESTINATION bin)
set_target_properties(circlescope PROPERTIES OUTPUT_NAME "ol-circlescope")

add_executable(scope scope.c)
target_link_libraries(scope ${JACK_LIBRARIES} m)
install(TARGETS scope RUNTIME DESTINATION bin)
set_target_properties(scope PROPERTIES OUTPUT_NAME "ol-scope")

add_executable(simple simple.c)
target_link_libraries(simple ol)
install(TARGETS simple RUNTIME DESTINATION bin)
set_target_properties(simple PROPERTIES OUTPUT_NAME "ol-simple")

add_executable(pong pong.c)
target_link_libraries(pong ol)
install(TARGETS pong RUNTIME DESTINATION bin)
set_target_properties(pong PROPERTIES OUTPUT_NAME "ol-pong")

if(ALSA_FOUND)
  add_executable(midiview midiview.c)
  target_link_libraries(midiview ol ${ALSA_LIBRARIES})
  install(TARGETS midiview RUNTIME DESTINATION bin)
  set_target_properties(midiview PROPERTIES OUTPUT_NAME "ol-midiview")
else()
  message(STATUS "Will NOT build midiview (ALSA missing)")
endif()

add_executable(harp harp.c)
target_link_libraries(harp ol)
install(TARGETS harp RUNTIME DESTINATION bin)
set_target_properties(harp PROPERTIES OUTPUT_NAME "ol-harp")

#add_subdirectory(27c3_slides)

# Note: this target needs modplug but I'm too lazy to add a module finder for
# it.
#add_subdirectory(lase_demo)
