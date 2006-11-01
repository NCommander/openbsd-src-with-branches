#	$OpenBSD$

.include <bsd.own.mk>
LIB=    stubs
SRCS=	setlocale.c db.c
NOPIC=	Yes
NOPROFILE=Yes
NOMAN=1

CFLAGS+=-Os -D__DBINTERFACE_PRIVATE

LIBCSRCDIR=${.CURDIR}/../../../lib/libc

.include <bsd.lib.mk>
