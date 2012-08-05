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

find_package (Ncurses)
if (NCURSES_FOUND)
    message ("-- ncurses found")
endif (NCURSES_FOUND)

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

# target_link_libraries (${NCURSES_LIBRARY})

add_definitions (-Wall -g -Wextra -std=c99 -lncursesw)

add_executable (tasknc tasknc.c)
