#	$NetBSD: Makefile,v 1.1 1997/10/04 09:11:21 mrg Exp $
#	$OpenBSD: Makefile,v 1.2 1999/01/21 05:47:39 d Exp $

PROG=	huntd
SRCS=	answer.c conf.c draw.c driver.c execute.c expl.c \
	extern.c makemaze.c shots.c terminal.c
MAN=	huntd.6
LDADD+=	-lwrap
DPADD+= ${LIBWRAP}

.include <bsd.prog.mk>
