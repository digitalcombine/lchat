# /lib/systemd/system/

EXTRA_DIST=lchatd lchatd.service.in

if FREEBSD
rcdata_DATA=lchatd
rcdatadir=${prefix}/etc/rc.d
endif

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
