#	$OpenBSD: Makefile,v 1.5 1997/09/21 11:51:21 deraadt Exp $

PROG=	tput
DPADD=	${LIBCURSES}
LDADD=	-lcurses
MLINKS=	tput.1 clear.1
LINKS= ${BINDIR}/tput ${BINDIR}/clear

.include <bsd.prog.mk>
