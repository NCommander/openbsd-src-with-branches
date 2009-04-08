#	$OpenBSD: Makefile,v 1.1 2002/02/08 20:12:37 deraadt Exp $

PROG=	grep
SRCS=	binary.c file.c grep.c mmfile.c queue.c util.c
MAN=
COPTS+=-Os -DNOZ
.PATH:  ${.CURDIR}/../../../usr.bin/grep

#LDADD=-lz
#DPADD=${LIBZ}

.include <bsd.prog.mk>
