# $OpenBSD$

.if defined(INCLUDE_FILES)
includes:
	@-for i in ${INCLUDE_FILES}; do \
		echo "installing `basename $$i`..." ;\
		cmp -s $$i ${DESTDIR}/usr/include/kerberosIV/$$i || \
		${INSTALL} ${INSTALL_COPY} -m 444 $$i ${DESTDIR}/usr/include/kerberosIV/`basename $$i` ;\
	done
.endif

