#	$OpenBSD: Makefile,v 1.8 1998/07/25 21:30:49 millert Exp $

PROG=	tput
DPADD=	${LIBCURSES}
LDADD=	-lcurses
MLINKS=	tput.1 clear.1
LINKS= ${BINDIR}/tput ${BINDIR}/clear

.include <bsd.prog.mk>
