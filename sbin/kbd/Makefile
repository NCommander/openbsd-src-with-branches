#	$OpenBSD: Makefile,v 1.17 2003/10/31 04:05:29 drahn Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "macppc") || (${MACHINE} == "pegasos") || \
    (${MACHINE} == "alpha") || (${MACHINE} == "hppa") || \
    (${MACHINE} == "sparc") || (${MACHINE} == "sparc64") || \
    (${MACHINE} == "vax")
    
PROG=	kbd

SRCS=	main.c kbd_wscons.c
LDADD=	-lkvm
DPADD=	${LIBKVM}

.else
NOPROG=	Yes
.endif

MAN=		kbd.8
MANSUBDIR=	alpha hppa i386 macppc sparc sparc64 vax pegosos amd64

.include <bsd.prog.mk>
