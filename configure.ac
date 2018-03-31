#                                                              -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([lchat], [1.2.1], [ron.rwsoft@gmail.com])
AM_INIT_AUTOMAKE
AM_SILENT_RULES
AC_CONFIG_SRCDIR([src/curses.cpp])
AC_CONFIG_MACRO_DIR([m4])

AC_ARG_ENABLE([testing],
  [AS_HELP_STRING([--enable-testing],
    [compile in testing code (default is no)])],
      [case "${enableval}" in
        yes) testing=true; AC_DEFINE(TESTING) ;;
        no)  testing=false ;;
        *) AC_MSG_ERROR([bad value ${enableval} for --enable-testing]) ;;
       esac], [testing=false])
AM_CONDITIONAL([TESTING], [test x$testing = xtrue])

AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug],
    [compile in debugging code (default is no)])],
      [case "${enableval}" in
        yes) debug=true; AC_DEFINE(DEBUG) ;;
        no)  debug=false ;;
        *) AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
       esac], [debug=false])
AM_CONDITIONAL([DEBUG], [test x$debug = xtrue])

AC_PROG_MKDIR_P

# Checks for programs.
AC_PROG_CXX

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h sys/socket.h unistd.h])
AC_CHECK_HEADERS([ncurses.h], [],
  [AC_CHECK_HEADERS([curses.h], [],
     [AC_MSG_ERROR([Curses library headers not found])])])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T

# Checks for library functions.
#AC_CHECK_LIB([ncurses], [main])
AC_CHECK_FUNCS([inet_ntoa memset select socket strerror])

AX_CHECK_COMPILE_FLAG([-std=c++11],
      [AX_APPEND_FLAG([-std=c++11], CXXFLAGS)],
      AC_MSG_ERROR(C++11 not supported by compiler))

AC_CONFIG_FILES([Makefile
                 include/Makefile
                 src/Makefile
                 man/Makefile])
AC_OUTPUT
