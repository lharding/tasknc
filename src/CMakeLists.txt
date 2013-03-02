add_executable (tasknc
    configure.c
    cursutil.c
    json.c
    main.c
    sort.c
    task.c
    tasklist.c
    )

install (TARGETS tasknc DESTINATION bin)
target_link_libraries (tasknc ${CURSES_NCURSES_LIBRARY})
