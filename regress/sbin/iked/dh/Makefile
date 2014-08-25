#	$OpenBSD$

# Test DH:

PROG=		dhtest
SRCS=		dh.c dhtest.c
TOPSRC=		${.CURDIR}/../../../../sbin/iked
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
.PATH:		${TOPSRC} ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPOBJ} -Wall

NOMAN=
LDADD+=		-lcrypto
DPADD+=		${LIBCRYPTO}
DEBUG=		-g

.include <bsd.regress.mk>
