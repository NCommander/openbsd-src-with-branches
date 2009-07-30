#	$OpenBSD$

.PATH:		${.CURDIR}/../ldpd

PROG=	ldpctl
SRCS=	buffer.c imsg.c log.c ldpctl.c parser.c
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../ldpd
MAN=	ldpctl.8

.include <bsd.prog.mk>
