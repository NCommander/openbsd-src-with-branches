# $OpenBSD: Makefile,v 1.3 2017/02/13 19:15:39 krw Exp $

.include <bsd.own.mk>

SRCS=	dhcrelay.c dispatch.c log.c bpf.c packet.c
PROG=	dhcrelay
MAN=	dhcrelay.8

CFLAGS+=-Wall
CFLAGS+=-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=-Wmissing-declarations
CFLAGS+=-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=-Wsign-compare

.include <bsd.prog.mk>
