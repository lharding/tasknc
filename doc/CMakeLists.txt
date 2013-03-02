add_custom_target (ManPages ALL)
set (IN_POD manual.pod)
set (OUT_MANPAGE tasknc.1)
add_custom_command (
    TARGET ManPages
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/${IN_POD}
    COMMAND pod2man ARGS -s 1 --section=1 --center="tasknc Manual" --name="tasknc" --release="tasknc ${VERSION}" ${CMAKE_CURRENT_SOURCE_DIR}/${IN_POD} 
    ${CMAKE_CURRENT_BINARY_DIR}/${OUT_MANPAGE} 
    OUTPUTS ${CMAKE_CURRENT_BINARY_DIR}/${OUT_MANPAGE}
    )

add_custom_command (
    TARGET ManPages
    SOURCE ManPages
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${OUT_MANPAGE}
    )


install ( FILES ${CMAKE_CURRENT_BINARY_DIR}/${OUT_MANPAGE}
    DESTINATION share/man/man1
    )
