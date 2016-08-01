#	$OpenBSD$

LIBCSRCDIR=${.CURDIR}/../../lib/libc
CFLAGS += -fpic
CPPFLAGS += -I${LIBCSRCDIR}/arch/m88k

SRCS+=	memcpy.c
