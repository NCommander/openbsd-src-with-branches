# $OpenBSD: Makefile,v 1.7 2006/10/14 20:36:40 deraadt Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "armish" || ${MACHINE} == "i386" || \
    ${MACHINE} == "hppa" || ${MACHINE} == "landisk" || \
    ${MACHINE} == "macppc" || ${MACHINE} == "socppc" || \
    ${MACHINE} == "sparc64" || ${MACHINE} == "zaurus"

PROG=	hotplugd
SRCS=	hotplugd.c

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations -Wredundant-decls
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual

.else

NOPROG=	yes

.endif

MAN=	hotplugd.8
MANSUBDIR=alpha amd64 armish i386 hppa landisk macppc socppc sparc64 zaurus

.include <bsd.prog.mk>
