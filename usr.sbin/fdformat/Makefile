#	$OpenBSD: Makefile,v 1.2 1996/06/28 00:53:37 downsj Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "sparc"
PROG=	fdformat
DPADD=	${LIBUTIL}
LDADD=	-lutil
.else
NOPROG=yes
.endif

MAN=fdformat.1

MANSUBDIR=i386 sparc

.include <bsd.prog.mk>
