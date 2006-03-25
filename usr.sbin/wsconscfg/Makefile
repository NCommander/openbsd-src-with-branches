#	$OpenBSD: Makefile,v 1.19 2005/01/14 22:39:40 miod Exp $

.if ${MACHINE} != "mvme68k" && ${MACHINE} != "mvme88k" && \
    ${MACHINE} != "vax"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8
MANSUBDIR=	alpha amd64 cats hp300 hppa i386 luna88k mac68k macppc \
		sgi sparc sparc64 zaurus

.include <bsd.prog.mk>
