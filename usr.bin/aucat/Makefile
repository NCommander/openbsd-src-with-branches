#	$OpenBSD: Makefile,v 1.8 2009/06/04 19:05:09 jsg Exp $

PROG=	aucat
SRCS=	aucat.c abuf.c aparams.c aproc.c dev.c midi.c file.c headers.c \
	safile.c miofile.c sock.c pipe.c listen.c opt.c wav.c legacy.c 
MAN=	aucat.1 midicat.1
LINKS=  ${BINDIR}/aucat ${BINDIR}/midicat
CFLAGS+= -DDEBUG -Wall -Wstrict-prototypes -Wundef
LDADD+=	-lsndio
.include <bsd.prog.mk>
