#                                                           -*- Makefile.am -*-

bin_PROGRAMS = lchat
sbin_PROGRAMS = lchatd
dist_pkglibexec_SCRIPTS = fortune-bot.sh

lchat_SOURCES = lchat.cpp autocomplete.cpp curses.cpp nstream.cpp
lchat_CPPFLAGS = -DSTATEDIR=\"@lchatstatedir@\" -I $(top_srcdir)/include/ \
	$(CURSES_CFLAGS) $(PTHREAD_CFLAGS)
lchat_LDADD = $(CURSES_LIBS) $(PTHREAD_LIBS)

lchatd_SOURCES = lchatd.cpp nstream.cpp
lchatd_CPPFLAGS = -DSTATEDIR=\"@lchatstatedir@\" -I $(top_srcdir)/include/
