#	$OpenBSD$

PROG=	faithd
SRCS=	faithd.c tcp.c ftp.c rsh.c

MAN=	faithd.8

#CPPFLAGS+= -DFAITH4

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
