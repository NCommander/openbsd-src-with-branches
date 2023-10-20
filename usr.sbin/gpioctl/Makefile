# $OpenBSD: Makefile,v 1.9 2020/05/16 16:37:50 martin Exp $

.if ${MACHINE} == "amd64" || ${MACHINE} == "arm64" || \
    ${MACHINE} == "armv7" || ${MACHINE} == "i386" || \
    ${MACHINE} == "macppc" || ${MACHINE} == "riscv64"

PROG=	gpioctl
SRCS=	gpioctl.c

CFLAGS+=-Wall
CFLAGS+=-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=-Wmissing-declarations
CFLAGS+=-Wshadow -Wpointer-arith -Wcast-qual

.else

NOPROG=	yes

.endif

MAN=	gpioctl.8
MANSUBDIR=amd64 arm64 armv7 i386 macppc riscv64

.include <bsd.prog.mk>
