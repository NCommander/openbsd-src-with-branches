#	$OpenBSD: Makefile,v 1.1 2002/02/08 20:12:37 deraadt Exp $

PROG=	gzip
SRCS=	main.c zopen.c gzopen.c
MAN=
COPTS+=-Os
.PATH:  ${.CURDIR}/../../../usr.bin/compress

LDADD=-lz
DPADD=${LIBZ}

.include <bsd.prog.mk>
