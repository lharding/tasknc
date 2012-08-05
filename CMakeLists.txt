cmake_minimum_required (VERSION 2.8)
project (tasknc)

add_definitions (-Wall -g -Wextra -lncursesw)
find_package (Curses)
set (CURSES_USE_NCURSES true)

add_executable(tasknc tasknc.c)
