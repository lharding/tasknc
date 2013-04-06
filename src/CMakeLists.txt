add_executable (tasknc
    configure.c
    cursutil.c
    history.c
    keybind.c
    main.c
    sort.c
    statusbar.c
    tasklist.c
    )

install (TARGETS tasknc DESTINATION bin)
target_link_libraries (tasknc ${CURSES_NCURSES_LIBRARY} task)
