#	$OpenBSD: Makefile,v 1.3.12.1 2003/10/10 17:04:13 drahn Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "alpha") || (${MACHINE} == "pegasos")
PROG=   iopctl
.else
NOPROG=
.endif

MAN=	iopctl.8

.include <bsd.prog.mk>
