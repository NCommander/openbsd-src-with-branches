#	$OpenBSD: Makefile,v 1.2 2003/05/06 00:23:03 millert Exp $

PROG=	more
MAN=
CFLAGS+=-D_USE_OLD_CURSES_ -I.
DPADD=	${LIBOCURSES}
LDADD=	-locurses
CLEANFILES+=morehelp.h

beforedepend: morehelp.h

morehelp.h: ${.CURDIR}/more.help
	sed 's/\(^.*$$\)/\"\1\",/' ${.CURDIR}/more.help > morehelp.h

.include <bsd.prog.mk>
