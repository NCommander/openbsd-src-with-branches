#	$OpenBSD$

CFLAGS += -fpic -msoft-float -I${LIBCSRCDIR}/arch/sparc
AFLAGS = ${CFLAGS}
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.include "${LIBCSRCDIR}/arch/sparc/Makefile.inc"
.PATH: ${LIBCSRCDIR}/arch/sparc/gen/
SRCS+=umul.S
