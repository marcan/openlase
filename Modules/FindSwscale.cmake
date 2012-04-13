# Locate libswscale (part of ffmpeg)
#
#  SWSCALE_FOUND - system has swscale
#  SWSCALE_INCLUDE_DIR - the swscale include directory
#  SWSCALE_LIBRARIES - the libraries needed to use swscale
#  SWSCALE_DEFINITIONS - Compiler switches required for using swscale

# Copyright (c) 2010, Maciej Mrozowski <reavertm@gmail.com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

include(FindPackageHandleStandardArgs)

find_package(PkgConfig)
pkg_check_modules(PC_SWSCALE libswscale)
set(SWSCALE_DEFINITIONS ${PC_SWSCALE_CFLAGS_OTHER})

find_library(SWSCALE_LIBRARIES swscale
    HINTS ${PC_SWSCALE_LIBDIR} ${PC_SWSCALE_LIBRARY_DIRS}
)

find_path(SWSCALE_INCLUDE_DIR swscale.h
    HINTS ${PC_SWSCALE_INCLUDEDIR} ${PC_SWSCALE_INCLUDE_DIRS}
    PATH_SUFFIXES libswscale
)

find_package_handle_standard_args(Swscale "Could not find libswscale; available at www.ffmpeg.org" SWSCALE_LIBRARIES SWSCALE_INCLUDE_DIR)

mark_as_advanced(SWSCALE_INCLUDE_DIR SWSCALE_LIBRARIES)