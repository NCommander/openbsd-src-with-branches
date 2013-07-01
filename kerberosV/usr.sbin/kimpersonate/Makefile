# $OpenBSD: Makefile,v 1.2 2013/06/19 19:36:59 robert Exp $

PROG=	kimpersonate
MPAGES=	kimpersonate.8
MSRC=	${KRB5DIR}/kuser

CFLAGS+=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/sl -I.
DPADD=	${LIBKAFS5} ${LIBKRB5} ${LIBHEIMNTLM} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCRYPT} ${LIBVERS}
LDADD=	-lkafs5 -lkrb5 -lroken -lasn1 -lcrypto \
	${LIBVERS} -lcom_err -lheimbase -lwind

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
