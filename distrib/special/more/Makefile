#	$OpenBSD: Makefile,v 1.1 2003/04/17 22:53:12 millert Exp $

PROG=	more
MAN=
CFLAGS+=-D_USE_OLD_CURSES_
DPADD=	${LIBOCURSES}
LDADD=	-locurses

.include <bsd.prog.mk>
