#                                                           -*- Makefile.am -*-

bin_PROGRAMS = lchat
sbin_PROGRAMS = lchatd
dist_pkglibexec_SCRIPTS = fortune-bot.sh

lchat_SOURCES = lchat.cpp curses.cpp nstream.cpp
lchat_CPPFLAGS = -I ../include/ $(CURSES_CFLAGS)
lchat_LDADD = $(CURSES_LIBS)

lchatd_SOURCES = lchatd.cpp nstream.cpp
lchatd_CPPFLAGS = -I ../include/
