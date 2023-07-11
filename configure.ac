#                                                              -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([lchat], [1.7.2], [ron@digitalcombine.ca])
AM_INIT_AUTOMAKE
AM_SILENT_RULES
AC_CONFIG_SRCDIR([src/lchatd.cpp])
AC_CONFIG_MACRO_DIR([m4])

# Platform detection.
AC_CANONICAL_HOST

# Some build options.
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

AC_ARG_WITH([systemd],
  [AS_HELP_STRING([--with-systemd],
    [Support systemd service for the local chat server])],
  [with_systemd=true],
  [])
AM_CONDITIONAL([SYSTEMD], [test x$with_systemd = xtrue])

AC_PROG_MKDIR_P

lchatstatedir='${localstatedir}/lib/lchat'
AC_SUBST(lchatstatedir)

# Checks for programs.
PKG_PROG_PKG_CONFIG([0.25])
AC_LANG(C++)
AC_PROG_CXX

# Checks for libraries.
PKG_CHECK_MODULES([CURSES], [ncursesw >= 5],
  [AC_DEFINE(HAVE_NCURSESW_H)],
  [PKG_CHECK_MODULES([CURSES], [ncurses >= 5], [AC_DEFINE(HAVE_CURSES_H)],
    [AC_MSG_ERROR([ncurses library not found])])])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([inet_ntoa memset select socket strerror])
AC_CHECK_FUNCS([gethostname])

AX_CHECK_COMPILE_FLAG([-std=c++17],
      [AX_APPEND_FLAG([-std=c++17], CXXFLAGS)],
      AC_MSG_ERROR(C++17 not supported by compiler))

AX_PTHREAD(, [AC_MSG_ERROR([Posix thread library not found])])

AM_CONDITIONAL([FREEBSD], [test x = y])
case $host_os in
  freebsd*)
    AM_CONDITIONAL([FREEBSD], [test x = x])
    # Package configure sometimes only links to ncurses instead of
    # ncursesw. This fixes that.
    AC_CHECK_LIB(ncursesw, main,
    	         [CURSES_LIBS=$(echo $CURSES_LIBS | \
		                sed 's/-lncurses/-lncursesw/')])
    ;;
esac

AC_CONFIG_FILES([Makefile
                 include/Makefile
                 src/Makefile
                 man/Makefile
                 extra/Makefile])
AC_OUTPUT
