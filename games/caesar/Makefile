#	$OpenBSD: Makefile,v 1.5 1998/02/26 07:58:07 deraadt Exp $

PROG=	caesar
MAN=	caesar.6
DPADD=	${LIBM}
LDADD=	-lm
MLINKS=	caesar.6 rot13.6
LINKS=	${BINDIR}/caesar ${BINDIR}/rot13

.include <bsd.prog.mk>
