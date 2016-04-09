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

check_include_files(malloc.h HAVE_MALLOC_H)
check_function_exists(memalign HAVE_MEMALIGN)
check_function_exists(posix_memalign HAVE_POSIX_MEMALIGN)
check_function_exists(_aligned_malloc HAVE_ALIGNED_MALLOC)
check_include_files(hersheyfont.h HAVE_HERSHEYFONT)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

include_directories (${CMAKE_SOURCE_DIR}/include ${CMAKE_CURRENT_BINARY_DIR})

set(TRACER_SOURCES "")
if(BUILD_TRACER)
  set(TRACER_SOURCES trace.c imgproc_sse2.asm)
  enable_language(ASM_YASM)
  message(STATUS "Will build tracer (SSE2 version)")
else()
  message(STATUS "Will NOT build tracer")
endif()

add_library (openlase SHARED libol.c text.c ilda.c ${TRACER_SOURCES} ${CMAKE_CURRENT_BINARY_DIR}/fontdef.c)
target_link_libraries (openlase ${CMAKE_THREAD_LIBS_INIT} m jack hersheyfont)
set_target_properties(openlase PROPERTIES VERSION 0 SOVERSION 0)
install(TARGETS openlase LIBRARY DESTINATION lib)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fontdef.c
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/genfont.py
    MAIN_DEPENDENCY laserfont.svg
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/genfont.py ${CMAKE_CURRENT_SOURCE_DIR}/laserfont.svg ${CMAKE_CURRENT_BINARY_DIR}/fontdef.c default_font)
