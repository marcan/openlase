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

add_library (openlase SHARED libol.c text.c ilda.c ${CMAKE_CURRENT_BINARY_DIR}/fontdef.c)
target_link_libraries (openlase ${CMAKE_THREAD_LIBS_INIT} m jack)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fontdef.c
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/genfont.py
    MAIN_DEPENDENCY laserfont.svg
    COMMAND python ${CMAKE_SOURCE_DIR}/tools/genfont.py ${CMAKE_CURRENT_SOURCE_DIR}/laserfont.svg ${CMAKE_CURRENT_BINARY_DIR}/fontdef.c default_font)
