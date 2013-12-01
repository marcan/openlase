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
install(TARGETS circlescope DESTINATION bin)

add_executable(scope scope.c)
target_link_libraries(scope ${JACK_LIBRARIES} m)
install(TARGETS scope DESTINATION bin)

add_executable(simple simple.c)
target_link_libraries(simple openlase)
install(TARGETS simple DESTINATION bin)

add_executable(spin-text spin-text.c)
target_link_libraries(spin-text openlase)
install(TARGETS spin-text DESTINATION bin)

add_executable(pong pong.c)
target_link_libraries(pong openlase)
install(TARGETS pong DESTINATION bin)

if(ALSA_FOUND)
  add_executable(midiview midiview.c)
  target_link_libraries(midiview openlase ${ALSA_LIBRARIES})
  install(TARGETS midiview DESTINATION bin)
else()
  message(STATUS "Will NOT build midiview (ALSA missing)")
endif()

add_executable(harp harp.c)
target_link_libraries(harp openlase)
install(TARGETS harp DESTINATION bin)

#add_subdirectory(27c3_slides)

# Note: this target needs modplug but I'm too lazy to add a module finder for
# it.
#add_subdirectory(lase_demo)
