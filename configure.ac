#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([tasknc], [0.8], [mjheagle8@gmail.com])
AM_INIT_AUTOMAKE([tasknc], [0.8])
AC_CONFIG_SRCDIR([src/])
AC_CONFIG_HEADERS()

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lncursesw':
AC_CHECK_LIB([ncursesw], [main])

# Checks for header files.
AC_CHECK_HEADERS([locale.h stdlib.h string.h unistd.h wchar.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset regcomp setlocale strchr strdup strndup strstr])

AC_OUTPUT(Makefile src/Makefile)
