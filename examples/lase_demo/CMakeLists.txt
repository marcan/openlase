include_directories (${CMAKE_SOURCE_DIR}/include)
link_directories (${CMAKE_BINARY_DIR}/libol)

add_executable(demo demo.c trace.c)
target_link_libraries(demo openlase modplug)

