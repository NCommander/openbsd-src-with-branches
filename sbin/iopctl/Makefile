#	$OpenBSD: Makefile,v 1.1.1.1 2001/07/05 02:07:46 mickey Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "powerpc") || \
    (${MACHINE} == "alpha")
PROG=   iopctl
.else
NOPROG=
.endif

MAN=	iopctl.8

.include <bsd.prog.mk>
