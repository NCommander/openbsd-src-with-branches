#	$OpenBSD: Makefile.inc,v 1.2 2002/12/09 20:56:34 drahn Exp $

CFLAGS += -fpic -msoft-float
AFLAGS += -D_STANDALONE
SRCS+= divsi3.S
ELF_LDFLAGS+=-z nocombreloc
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.PATH: ${LIBCSRCDIR}/arch/arm/gen/
