include_directories("inc")

file(GLOB XCLIPPIPE_SRCS *.c *.h inc/*.h)

add_executable (xclippipe xclippipe.c)
add_definitions (${X11_DEFINITIONS} ${XCB_DEFINITIONS})
target_link_libraries (xclippipe ${X11_LIBRARIES} ${XCB_LIBRARIES})

install (TARGETS xclippipe DESTINATION bin)
