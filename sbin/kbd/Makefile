#	$OpenBSD: Makefile,v 1.20 2004/01/29 08:36:13 mickey Exp $

#
# Be sure to change src/distrib/special/kbd/Makefile as well if you add
# architectures to this list!
#
.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "macppc") || (${MACHINE} == "alpha") || \
    (${MACHINE} == "hppa") || (${MACHINE} == "sparc") || \
    (${MACHINE} == "sparc64") || (${MACHINE} == "vax") || \
    (${MACHINE} == "cats")
    
PROG=	kbd

SRCS=	main.c kbd_wscons.c
LDADD=	-lkvm
DPADD=	${LIBKVM}

.else
NOPROG=	Yes
.endif

MAN=		kbd.8
MANSUBDIR=	alpha hppa i386 macppc sparc sparc64 vax amd64 cats

.include <bsd.prog.mk>
