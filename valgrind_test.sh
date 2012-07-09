#!/bin/bash

errorfile="valgrind_errors"
run="./tasknc -d"
valgrind_options="--tool=memcheck --leak-check=full -v --show-reachable=yes --track-origins=yes"

if [ -e $errorfile ]; then
    rm $errorfile
fi

valgrind $valgrind_options --log-file="$errorfile" $run
vim "$errorfile"
