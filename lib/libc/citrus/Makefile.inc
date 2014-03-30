#	$OpenBSD$

# citrus sources
.PATH: ${LIBCSRCDIR}/citrus

SRCS+=	citrus_ctype.c citrus_none.c citrus_utf8.c
CFLAGS+=-I${.CURDIR}
