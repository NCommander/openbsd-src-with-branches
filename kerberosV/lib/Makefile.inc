# $OpenBSD: Makefile.inc,v 1.1 2000/02/25 16:35:24 hin Exp $

.if defined(INCLUDE_FILES)
INCLUDE_DIR?=	${DESTDIR}/usr/include/kerberosV
includes:	$(INCLUDE_FILES)
	mkdir -p ${INCLUDE_DIR}
	-for i in ${INCLUDE_FILES}; do \
		echo "installing `basename $$i` in ${INCLUDE_DIR}" ;\
		cmp -s $$i ${INCLUDE_DIR}/$$i || \
		${INSTALL} ${INSTALL_COPY} -m 444 $$i ${INCLUDE_DIR}/`basename $$i` ;\
	done
.endif

