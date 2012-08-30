#!/bin/bash

errorfile="valgrind_errors"
run="../tasknc -l8"
valgrind_options="--tool=memcheck --leak-check=full -v --show-reachable=yes --track-origins=yes --suppressions=valgrind.supp --vgdb=yes --vgdb-error=0"

if [ -e $errorfile ]; then
    rm $errorfile
fi

valgrind $valgrind_options --log-file="$errorfile" $run
vim "$errorfile" "+set ft="
