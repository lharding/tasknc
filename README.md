task ncurses
===================

by mjheagle
-----------

About
-----

`tasknc` is a ncurses wrapper for [task warrior](http://taskwarrior.org/projects/show/taskwarrior) written in C.  It aims to provide an interface like the excellent ncurses programs `mutt` and `ncmpcpp`.

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
