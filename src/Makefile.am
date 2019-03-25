#                                                           -*- Makefile.am -*-

bin_PROGRAMS = lchat
sbin_PROGRAMS = lchatd

lchat_SOURCES = lchat.cpp curses.cpp nstream.cpp
lchat_CPPFLAGS = -I ../include/
lchat_LDADD = -lncursesw

lchatd_SOURCES = lchatd.cpp nstream.cpp
lchatd_CPPFLAGS = -I ../include/
