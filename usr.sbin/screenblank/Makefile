#	$OpenBSD: Makefile,v 1.3 2000/05/18 16:46:38 espie Exp $

.if ${MACHINE} == "sparc" || ${MACHINE} == "sun3"
PROG=	screenblank

LDADD+=	-lm -lutil
DPADD+=	${LIBM} ${LIBUTIL}
.else
NOPROG=yes
.endif

MAN=screenblank.1
MANSUBDIR=sparc sun3
.include <bsd.prog.mk>
