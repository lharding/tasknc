cmake_minimum_required (VERSION 2.8)
include_directories (${CMAKE_SOURCE_DIR}
                     ${CMAKE_SOURCE_DIR}/src)

set (src_files color.c
               command.c
               common.c
               keys.c
               log.c
               pager.c
               tasklist.c
               tasknc.c
               tasks.c
               test.c)

find_package (Curses)
message ("-- looking for curses")
set (CURSES_NEED_NCURSES)
set (CURSES_NEED_WIDE)
if (CURSES_FOUND)
    message ("-- curses found")
    message ("-- curses headers: ${CURSES_HEADERS}")
    message ("-- curses library: ${CURSES_LIBRARY}")
    message ("-- ncurses headers: ${NCURSES_HEADERS}")
    message ("-- ncurses library: ${NCURSES_LIBRARY}")
    message ("-- ncurses wide headers: ${NCURSESW_HEADERS}")
    message ("-- ncurses wide library: ${NCURSESW_LIBRARY}")
else (CURSES_FOUND)
    message ("-- curses not found!")
endif (CURSES_FOUND)

# if (NCURSES_FOUND)
#   check_include_files ("ncursesw/ncurses.h" NCURSESW_HEADERS)
#   if (NCURSESW_HEADERS)
#     add_definitions (-DHAVE_NCURSESW_CURSES_H)
#   else (NCURSESW_HEADERS)
#     check_include_files ("ncurses.h" NCURSES_HEADERS)
#     if (NCURSES_HEADERS)
#       add_definitions (-DHAVE_NCURSES_H)
#     endif (NCURSES_HEADERS)
#   endif (NCURSESW_HEADERS)
# endif (NCURSES_FOUND)

target_link_libraries (${CURSES_LIBRARY})

add_definitions (-Wall -g -Wextra -std=c99)

add_executable (tasknc tasknc.c)
