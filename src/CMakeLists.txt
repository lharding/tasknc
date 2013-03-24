add_executable (tasknc
    configure.c
    cursutil.c
    main.c
    sort.c
    tasklist.c
    keybind.c
    )

install (TARGETS tasknc DESTINATION bin)
target_link_libraries (tasknc ${CURSES_NCURSES_LIBRARY} task)
