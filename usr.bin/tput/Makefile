#	$OpenBSD: Makefile,v 1.7 1998/07/25 20:47:31 downsj Exp $

PROG=	tput
CFLAGS+= -D_USE_OLD_CURSES_
DPADD=	${LIBOLDCURSES}
LDADD=	-locurses
MLINKS=	tput.1 clear.1
LINKS= ${BINDIR}/tput ${BINDIR}/clear

.include <bsd.prog.mk>
