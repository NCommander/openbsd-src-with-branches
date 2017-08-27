#	$OpenBSD: Makefile.inc,v 1.6 2016/08/15 03:09:02 guenther Exp $

CFLAGS += -fpic
CFLAGS+=-m4-nofpu
AFLAGS += -D_STANDALONE
LDADD=	`$(CC) -print-libgcc-file-name`
