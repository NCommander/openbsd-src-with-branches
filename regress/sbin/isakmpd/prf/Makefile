#	$OpenBSD: Makefile,v 1.7 2004/02/25 16:01:29 hshoexer Exp $
#	$EOM: Makefile,v 1.6 2000/03/28 21:22:07 ho Exp $

# Test PRF:

PROG=		prftest
SRCS=		prf.c hash.c log.c prftest.c conf.c sysdep.c util.c
TOPSRC=		${.CURDIR}/../../../../sbin/isakmpd
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
OS!=		awk '/^OS=/ { print $$2 }' ${.CURDIR}/../../Makefile
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/openbsd ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/openbsd -I${TOPOBJ} -Wall
NOMAN=
DEBUG=		-g

.include <bsd.regress.mk>
