#	$OpenBSD: Makefile,v 1.1 2022/01/05 09:59:39 jsing Exp $

PROG=		apitest
LDADD=		-lssl -lcrypto
DPADD=		${LIBSSL} ${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror
CFLAGS+=	-DCERTSDIR=\"${.CURDIR}/../../libssl/certs\"

.include <bsd.regress.mk>
