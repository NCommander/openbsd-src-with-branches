#       $OpenBSD$

NOMAN=	# defined

PROG=	skeytest
CFLAGS+=-Wall
DPADD+=	${LIBSKEY}
LDADD+=	-lskey

skey-regress: skeytest
	./${PROG}

REGRESS_TARGETS=skey-regress
.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>

