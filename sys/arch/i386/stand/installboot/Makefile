#	$OpenBSD: Makefile,v 1.7 2000/05/25 21:55:01 deraadt Exp $

MAN=	installboot.8
MANSUBDIR=/i386

.if ${MACHINE} == "i386"
PROG=	installboot

LDADD=	-lutil
DPADD=	${LIBUTIL}
# Need this to work in the miniroot
LDSTATIC=	-static

SADIR=	${.CURDIR}/..
.else
NOPROG=
.endif

.include <bsd.prog.mk>
