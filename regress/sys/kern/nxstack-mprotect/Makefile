#	$OpenBSD: Makefile,v 1.1 2002/07/27 06:52:37 mickey Exp $

.if ${MACHINE} == "sparc64" || ${MACHINE} == "sparc" || \
    ${MACHINE} == "alpha" || ${MACHINE} == "macppc" || \
    ${MACHINE} == "mvmeppc" || ${MACHINE} == "i386" || \
    ${MACHINE} == "hppa"

PROG=	nxstack-mprotect
LDSTATIC= ${STATIC}

.else
REGRESSTARGETS=		# this architecture has no test for this
.endif

.include <bsd.regress.mk>
