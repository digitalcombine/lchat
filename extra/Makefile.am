# /lib/systemd/system/

EXTRA_DIST=lchatd.service.in
systemddata_DATA=lchatd.service
systemddatadir="/lib/systemd/system"

if SYSTEMD
# If systemd was enabled in configure

edit = sed -e 's|@sbindir[@]|$(sbindir)|g'

lchatd.service: Makefile
	rm -f $@ $@.tmp
	srcdir=''; \
		test -f ./$@.in || srcdir=$(srcdir)/; \
		$(edit) $${srcdir}$@.in > $@.tmp

	mv $@.tmp $@
	chmod a-x,g-w,o-w $@

lchatd.service: lchatd.service.in
endif
