#	$OpenBSD: Makefile,v 1.1 2005/08/03 22:33:58 espie Exp $

.include <bsd.own.mk>
LIB=    stubs
SRCS=	setlocale.c db.c sha2.c
NOPIC=	Yes
NOPROFILE=Yes
NOMAN=1

CFLAGS+=-Os -D__DBINTERFACE_PRIVATE -DSHA256_ONLY

LIBCSRCDIR=${.CURDIR}/../../../lib/libc

.PATH: ${.CURDIR}/../../../lib/libc/hash

.include <bsd.lib.mk>
