# CMakeLists for tasknc
CMake_minimum_required (VERSION 2.6)
project (tasknc C)
exec_program(
    "git"
    ${CMAKE_CURRENT_SOURCE_DIR}
    ARGS "describe"
    OUTPUT_VARIABLE VERSION )

#CMAKE pkg finder scripts
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

#FIND ncursesw
SET(CURSES_NEED_NCURSES True)
SET(CURSES_NEED_WIDE True)
find_package( curses )

#set up includes
include_directories(
    ${CURSES_NCURSES_INCLUDE_PATH}
)
#set up uninstall target
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
@ONLY)
add_custom_target(uninstall
    ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
)


add_subdirectory(src)
add_subdirectory(doc)

install(FILES config  DESTINATION share/tasknc)
