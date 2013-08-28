#	$OpenBSD: Makefile,v 1.17 2011/12/09 14:36:42 ratchov Exp $

PROG=	sndiod
SRCS=	abuf.c dev.c dsp.c file.c listen.c midi.c miofile.c opt.c siofile.c sndiod.c sock.c utils.c
MAN=	sndiod.1
CFLAGS+= -Wall -Wstrict-prototypes -Wundef -DDEBUG -I${.CURDIR}/../../lib/libsndio
LDADD+=	-lsndio
.include <bsd.prog.mk>
