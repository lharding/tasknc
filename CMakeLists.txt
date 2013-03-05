# CMakeLists for tasknc
CMake_minimum_required (VERSION 2.6)
project (tasknc C)
exec_program (
    "git"
    ${CMAKE_CURRENT_SOURCE_DIR}
    ARGS "describe"
    OUTPUT_VARIABLE VERSION
    )

#cmake pkg finder scripts
list (APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

#find ncursesw
set (CURSES_NEED_NCURSES True)
set (CURSES_NEED_WIDE True)
find_package (curses)

#set up configuration header
configure_file (
    "${PROJECT_SOURCE_DIR}/src/cmake.h.in"
    "${PROJECT_SOURCE_DIR}/src/cmake.h"
    )

#set up includes
include_directories (
    ${CURSES_NCURSES_INCLUDE_PATH}
    "${PROJECT_SOURCE_DIR}/libtask/"
    )

#set up uninstall target
configure_file (
    ${CMAKE_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    @ONLY)
add_custom_target( uninstall
    ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    )

#add subdirectories to build
add_subdirectory (libtask)
add_subdirectory (src)
add_subdirectory (doc)

#files to install
install(FILES config  DESTINATION share/tasknc)
