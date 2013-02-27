add_executable(tasknc 
color.c
command.c
common.c
formats.c
keys.c
log.c
pager.c
sort.c
statusbar.c
tasklist.c
tasknc.c
tasks.c
test.c )

install(TARGETS tasknc DESTINATION bin)
target_link_libraries(tasknc ${CURSES_NCURSES_LIBRARY})
