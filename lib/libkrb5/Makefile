# $OpenBSD$

KRB5DIR=	${.CURDIR}/../../kerberosV
.include "Makefile.common.inc"

LIB	= newkrb5

.include "Makefile.asn1.inc"
.include "Makefile.hdb.inc"
.include "Makefile.kafs.inc"
.include "Makefile.roken.inc"
.include "Makefile.krb5.inc"

includes:
	pwd
	@for i in $(INCLUDE_FILES); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/kerberosV/$$i || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
		$$i ${DESTDIR}/usr/include/kerberosV"; \
	    echo $$j; \
	pwd; \
	    eval "$$j"; \
	done


.include <bsd.lib.mk>
