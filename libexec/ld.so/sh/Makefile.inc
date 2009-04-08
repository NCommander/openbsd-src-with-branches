#	$OpenBSD: Makefile.inc,v 1.3 2007/03/03 19:40:53 drahn Exp $

CFLAGS += -fpic
CFLAGS+=-m4-nofpu
AFLAGS += -D_STANDALONE
AFLAGS += -I${.CURDIR}/../../lib/libc/arch/sh
ELF_LDFLAGS+=-z nocombreloc
LIBCSRCDIR=${.CURDIR}/../../lib/libc
LDADD=	`$(CC) -print-libgcc-file-name`
