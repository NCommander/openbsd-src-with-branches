#	$OpenBSD: Makefile,v 1.4 1999/07/08 20:11:54 deraadt Exp $


MAN=	installboot.8
MANSUBDIR=/sparc

.if ${MACHINE} == "sparc"
BINDIR=	/usr/sbin
PROG=	installboot
.else
NOPROG=
.endif

.include <bsd.prog.mk>
