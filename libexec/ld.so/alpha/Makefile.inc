#	$OpenBSD$

CFLAGS += -fpic -mno-fp-regs
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.include "${LIBCSRCDIR}/arch/alpha/Makefile.inc"
