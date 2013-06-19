# $OpenBSD: Makefile,v 1.1 2013/06/17 19:11:46 robert Exp $

PROG=	kgetcred
CFLAGS+=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken
DPADD=	${LIBKRB5} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBVERS}
LDADD=	-lkrb5 -lroken -lasn1 -lcrypto ${LIBVERS} -lheimbase -lcom_err \
	-lwind

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
