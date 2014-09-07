#	$OpenBSD: Makefile.inc,v 1.1 2004/08/11 17:11:45 pefo Exp $

# CFLAGS += -fpic -msoft-float
LIBCSRCDIR=${.CURDIR}/../../lib/libc
CPPFLAGS += -I${LIBCSRCDIR}/arch/mips64
