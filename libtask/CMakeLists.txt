ADD_LIBRARY (task SHARED
    task.c
    json.c
    )

install (FILES task.h DESTINATION include)
install (TARGETS task DESTINATION lib)
