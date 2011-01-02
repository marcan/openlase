

find_package(Curses REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/tools ${CURSES_INCLUDE_DIR})

add_executable(slides
	main.c 3ddemos.c static.c metaballs.c circlescope.c avstream.c video.c pong.c
	${CMAKE_SOURCE_DIR}/tools/trace.c

	${CMAKE_CURRENT_BINARY_DIR}/openlase-logo.ild
	${CMAKE_CURRENT_BINARY_DIR}/27c3-logo.ild
	${CMAKE_CURRENT_BINARY_DIR}/jack-logo.ild
	${CMAKE_CURRENT_BINARY_DIR}/output.ild
)

target_link_libraries(slides openlase avformat avcodec ${CURSES_LIBRARIES})

function(svg2ild NAME)
	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.ild
	    DEPENDS ${CMAKE_SOURCE_DIR}/tools/svg2ild.py
	    MAIN_DEPENDENCY ${NAME}.svg
	    DEPENDS ${NAME}.svg ${NAME}.cfg
	    COMMAND python ${CMAKE_SOURCE_DIR}/tools/svg2ild.py -q ${ARGN} -cfg ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}.cfg ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}.svg ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.ild)
endfunction()

svg2ild(openlase-logo -noopt)
svg2ild(27c3-logo)
svg2ild(jack-logo)
svg2ild(output)
