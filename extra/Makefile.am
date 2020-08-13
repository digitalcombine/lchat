# /lib/systemd/system/

EXTRA_DIST=lchatd.service.in

if SYSTEMD
# If systemd was enabled in configure

systemddata_DATA=lchatd.service
systemddatadir=`pkg-config systemd --variable=systemdsystemunitdir`

edit = sed -e 's|@sbindir[@]|$(sbindir)|g'

lchatd.service: Makefile lchatd.service.in
	rm -f $@ $@.tmp
	srcdir=''; \
		test -f ./$@.in || srcdir=$(srcdir)/; \
		$(edit) $${srcdir}$@.in > $@.tmp

	mv $@.tmp $@
	chmod a-x,g-w,o-w $@

lchatd.service: lchatd.service.in
endif
