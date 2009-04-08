#	$OpenBSD: Makefile,v 1.6 2008/10/27 18:02:14 miod Exp $

PROG=	aucat
SRCS=	aucat.c abuf.c aparams.c aproc.c dev.c file.c headers.c \
	safile.c sock.c pipe.c listen.c wav.c legacy.c 
CFLAGS+= -DDEBUG -Wall -Wstrict-prototypes -Werror -Wundef
LDADD+=	-lsndio
.include <bsd.prog.mk>
