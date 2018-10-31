#	$OpenBSD: Makefile,v 1.1 2014/07/24 19:21:06 miod Exp $

.PATH: ${.CURDIR}/../../../usr.sbin/eeprom

.if ${MACHINE} == "macppc" || ${MACHINE} == "sparc64"
PROG=	eeprom

SRCS=	main.c
SRCS+=	ophandlers.c optree.c

CFLAGS+=	-DSMALL
.else
NOPROG=yes
.endif

.include <bsd.prog.mk>
