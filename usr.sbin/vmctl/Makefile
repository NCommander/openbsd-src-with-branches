#	$OpenBSD: Makefile,v 1.4 2017/07/15 05:05:36 pd Exp $

.if ${MACHINE} == "amd64" || ${MACHINE} == "i386"

.PATH:	${.CURDIR}/../vmd

PROG=	vmctl
SRCS=	vmctl.c main.c atomicio.c
SRCS+=	vioqcow2.c vioraw.c log.c
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../vmd
LDADD+=	-lutil -lpthread
DPADD+= ${LIBUTIL} ${LIBPTHREAD}
PATH+=../vmd

.else

NOPROG= yes

.endif

MAN= vmctl.8

.include <bsd.prog.mk>
