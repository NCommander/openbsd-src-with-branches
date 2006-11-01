#       $OpenBSD: Makefile,v 1.1 2003/06/15 16:40:27 mickey Exp $

PROG=	skeytest
CFLAGS+=-Wall
LDADD+=	-lskey
DPADD+=	${LIBSKEY}

skey-regress: skeytest
	./${PROG}

REGRESS_TARGETS=skey-regress
.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>

