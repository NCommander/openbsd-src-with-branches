#	$OpenBSD: Makefile,v 1.21 2006/05/09 21:17:13 miod Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
   ${MACHINE} == "cats" || ${MACHINE} == "hp300" || \
   ${MACHINE} == "hppa" || ${MACHINE} == "i386" || \
   ${MACHINE} == "luna88k" || ${MACHINE} == "mac68k" || \
   ${MACHINE} == "macpppc" || ${MACHINE} == "sgi" || \
   ${MACHINE} == "sparc" || ${MACHINE} == "sparc64" || \
   ${MACHINE} == "zaurus"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8
MANSUBDIR=	alpha amd64 cats hp300 hppa i386 luna88k mac68k macppc \
		sgi sparc sparc64 zaurus

.include <bsd.prog.mk>
