#	$OpenBSD: Makefile,v 1.21 2013/11/21 08:15:46 ratchov Exp $

PROG=	aucat
SRCS=	abuf.c afile.c aucat.c dsp.c utils.c
MAN=	aucat.1
CFLAGS+=-DDEBUG -I${.CURDIR}/../../lib/libsndio
COPTS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wundef
LDADD+=	-lsndio
.include <bsd.prog.mk>
