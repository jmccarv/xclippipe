cmake_minimum_required (VERSION 2.6)
project (xclippipe)

set (VERSION_MAJOR 0)
set (VERSION_MINOR 13)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

find_package(X11 REQUIRED)
find_package(XCB REQUIRED)

include_directories(${X11_INCLUDE_DIRS} ${XCB_INCLUDE_DIRS})

add_definitions (-Wall -DVERSION_MAJOR=${VERSION_MAJOR} -DVERSION_MINOR=${VERSION_MINOR})

add_subdirectory(src)
add_subdirectory(contrib)
