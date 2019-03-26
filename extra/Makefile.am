# /lib/systemd/system/

EXTRA_DIST=lchatd.service.in

if SYSTEMD
install-data-local: lchatd.service
	$(MKDIR_P) "$(DESTDIR)/lib/systemd/system"
	$(INSTALL_SCRIPT) 'lchatd.service' '$(DESTDIR)/lib/systemd/system'

edit = sed -e 's|@sbindir[@]|$(sbindir)|g'

lchatd.service: Makefile
	rm -f $@ $@.tmp
	srcdir=''; \
		test -f ./$@.in || srcdir=$(srcdir)/; \
		$(edit) $${srcdir}$@.in >$@.tmp

	chmod a-xw $@.tmp
	mv $@.tmp $@

lchatd.service: lchatd.service.in
endif
