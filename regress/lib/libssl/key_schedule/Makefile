# $OpenBSD$

PROG=	key_schedule
LDADD=	${SSL_INT} -lcrypto
DPADD=	${LIBCRYPTO} ${LIBSSL}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Wundef -Werror -I$(BSDSRCDIR)/lib/libssl

.include <bsd.regress.mk>
