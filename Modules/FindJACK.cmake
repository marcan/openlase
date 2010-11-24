# - Try to find JACK
# Once done, this will define
#
#  JACK_FOUND - system has JACK
#  JACK_INCLUDE_DIRS - the JACK include directories
#  JACK_LIBRARIES - link these to use JACK

include(LibFindMacros)

# Dependencies

set(JACK_PKGCONF_INCLUDE_DIRS /usr/include)
set(JACK_PKGCONF_LIBRARY_DIRS /usr/lib)
set(JACK_LIBRARY_NAME jack)

# Include dir
find_path(JACK_INCLUDE_DIR
  NAMES jack/jack.h
  PATHS ${JACK_PKGCONF_INCLUDE_DIRS}
)

# Finally the library itself
find_library(JACK_LIBRARY
  NAMES ${JACK_LIBRARY_NAME}
  PATHS ${JACK_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(JACK_PROCESS_INCLUDES JACK_INCLUDE_DIR)
set(JACK_PROCESS_LIBS JACK_LIBRARY)
libfind_process(JACK)
