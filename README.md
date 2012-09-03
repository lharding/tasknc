task ncurses
===================

by mjheagle
-----------

About
-----

Thank you for trying `tasknc`!
`tasknc` is a ncurses wrapper for [task warrior](http://taskwarrior.org/projects/show/taskwarrior) written in C.
It aims to provide an interface like the excellent ncurses programs `mutt` and `ncmpcpp`.

Installation
------------

Installation can be performed by pasting the commands below:

    git clone https://github.com/mjheagle8/tasknc.git
    cd tasknc.git
    make
    sudo make install

Requirements
------------

[task warrior](http://taskwarrior.org/projects/show/taskwarrior) is the backend on which tasknc relies.  In Arch Linux, this is provided by the `task` package.

`pod2man` is required to generate the manual page.  In Arch Linux, this is provided by the `perl` package.

`ncursesw` headers are required to compile with unicode support.  In Arch Linux, this is provided by the `ncurses` package.

Bugs
----
`tasknc` is still in active development, and there are still some bugs.
Reporting bugs is very helpful to development!
Please report these bugs on [github](https://github.com/mjheagle8/tasknc/issues?page=1&state=open).
If you experience a segmentation fault, it would be fantastic if you could go into the test directory and run the `valgrind_test.sh` script and attach the resulting `valgrind_errors` file.

Contributing
------------
Contribution in the form of feedback, patches, bug reports, etc. are greatly appreciated.
