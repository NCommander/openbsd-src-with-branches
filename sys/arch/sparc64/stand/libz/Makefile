#	$OpenBSD: Makefile,v 1.1 1997/09/17 10:46:24 downsj Exp $

LIB=	z

.PATH:  ${.CURDIR}/../../../../lib/libz

CLEANFILES+= machine

CFLAGS= -nostdinc ${CEXTRAFLAGS} ${AFLAGS} -O2 -D_ZLIB_PRIVATE -I${.CURDIR}/../../../.. -I${.CURDIR}/../../../../lib/libz -I${.CURDIR}/.

SRCS=	adler32.c crc32.c infblock.c infcodes.c inffast.c \
	inflate.c inftrees.c infutil.c

.NOPATH:machine
${SRCS}: machine

${OBJS}:	${.CURDIR}/../Makefile.inc

install:

NOPIC=
NOPROFILE=

.include <bsd.lib.mk>

machine:
	rm -f $@
	ln -s ${.CURDIR}/../../include $@
