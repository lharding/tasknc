task ncurses
===================

Thank you for trying `tasknc`!
`tasknc` is a ncurses wrapper for [taskwarrior] written in C.
It aims to provide an interface like the excellent ncurses programs `mutt` and `ncmpcpp`.

Installation
------------

`tasknc` can easily be installed from source:

    git clone https://github.com/lharding/tasknc.git
    cd tasknc
    make
    sudo make install

It is also in the [AUR](https://aur.archlinux.org/packages/taskwarrior-tasknc-git).

Requirements
------------

[taskwarrior] is the backend on which tasknc relies.
In Arch Linux, this is provided by the [`task`](https://www.archlinux.org/packages/community/x86_64/task) package.

`pod2man` is required to generate the manual page.
In Arch Linux, this is provided by the `perl` package.

`ncurses-dev` meta-package is required to compile.
In Debian Linux: `sudo apt install ncurses-dev`

`ncursesw` headers are required to compile with unicode support. 
In Arch Linux, this is provided by the `ncurses` package. In Debian Linux the package is `libncursesw5-dev`

Bugs
----

`tasknc` is still in active development, and there are still some bugs.
Reporting bugs is very helpful to development!

Please report these bugs on [github](https://github.com/lharding/tasknc/issues).
If you experience a segmentation fault, it would be fantastic if you could go into the test directory and run the `valgrind_test.sh` script and attach the resulting `valgrind_errors` file.

Contributing
------------

Contribution in the form of feedback, patches, bug reports, etc. is greatly appreciated.

Original code by [mjheagle8](https://github.com/mjheagle8). Special thanks to [matthiasbeyer](https://github.com/matthiasbeyer) for a massive code overhaul.

[taskwarrior]: http://taskwarrior.org
