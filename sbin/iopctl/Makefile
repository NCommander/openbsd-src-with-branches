#	$OpenBSD: Makefile,v 1.4 2003/10/31 04:05:29 drahn Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "macppc") || (${MACHINE} == "pegasos") || \
    (${MACHINE} == "alpha")
PROG=   iopctl
.else
NOPROG=
.endif

MAN=	iopctl.8

.include <bsd.prog.mk>
