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

include(CheckSymbolExists)

check_include_file(malloc.h HAVE_MALLOC_H)
check_symbol_exists(memalign malloc.h HAVE_MEMALIGN)
check_symbol_exists(posix_memalign stdlib.h HAVE_POSIX_MEMALIGN)
check_symbol_exists(_aligned_malloc malloc.h HAVE_ALIGNED_MALLOC)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

include_directories (${CMAKE_SOURCE_DIR}/include ${CMAKE_CURRENT_BINARY_DIR}
	${JACK_INCLUDE_DIR})

set(TRACER_SOURCES "")
if(BUILD_TRACER)
  set(TRACER_SOURCES trace.c imgproc_sse2.asm)
  enable_language(ASM_YASM)
  message(STATUS "Will build tracer (SSE2 version)")
else()
  message(STATUS "Will NOT build tracer")
endif()

add_library (ol SHARED libol.c text.c ilda.c ${TRACER_SOURCES} ${CMAKE_CURRENT_BINARY_DIR}/fontdef.c)
target_link_libraries (ol ${CMAKE_THREAD_LIBS_INIT} m jack)
set_target_properties(ol PROPERTIES VERSION 0 SOVERSION 0)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fontdef.c
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/genfont.py
    MAIN_DEPENDENCY laserfont.svg
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/genfont.py ${CMAKE_CURRENT_SOURCE_DIR}/laserfont.svg ${CMAKE_CURRENT_BINARY_DIR}/fontdef.c default_font)

install(TARGETS ol LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
