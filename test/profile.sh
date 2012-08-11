#!/bin/bash
rm callgrind*
valgrind --tool=callgrind ../tasknc
callgrind_annotate --inclusive=yes callgrind.* | less
