#	$OpenBSD: Makefile,v 1.4 2023/04/23 18:59:41 tb Exp $

PROG=	clienttest
LDADD=	${SSL_INT} -lcrypto
DPADD=	${LIBSSL} ${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

# Disable for now for upcoming changes. This needs to be easier to deal with.
REGRESS_EXPECTED_FAILURES+=run-regress-clienttest

.include <bsd.regress.mk>
