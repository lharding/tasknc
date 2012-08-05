cmake_minimum_required (VERSION 2.8)
project (tasknc)

SET (MAN1DIR share/man/man1   CACHE STRING "Installation directory for man pages, section 1")
SET (DOCDIR  share/doc/tasknc CACHE STRING "Installation directory for doc files")
SET (BINDIR  bin)

add_definitions (-Wall -g -Wextra -lncursesw)
find_package (Curses)
set (CURSES_USE_NCURSES true)

add_subdirectory(src)
