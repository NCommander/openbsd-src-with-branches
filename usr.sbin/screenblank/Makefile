#	$OpenBSD: Makefile,v 1.2 1997/09/21 11:44:20 deraadt Exp $

.if ${MACHINE} == "sparc" || ${MACHINE} == "sun3"
PROG=	screenblank

DPADD=	${LIBM}
LDADD=	-lm
.else
NOPROG=yes
.endif

MAN=screenblank.1
MANSUBDIR=sparc sun3
.include <bsd.prog.mk>
