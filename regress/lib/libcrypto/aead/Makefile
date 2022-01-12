#	$OpenBSD: Makefile,v 1.3 2014/07/08 15:53:52 jsing Exp $

PROG=	aeadtest
#LDADD=	-lcrypto
LDADD=	${CRYPTO_INT} # XXX link dynamically after bump
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -DLIBRESSL_CRYPTO_INTERNAL -Werror

REGRESS_TARGETS=regress-aeadtest

regress-aeadtest: ${PROG}
	./${PROG} ${.CURDIR}/aeadtests.txt

.include <bsd.regress.mk>
