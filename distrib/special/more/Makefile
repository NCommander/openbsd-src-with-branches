#	$OpenBSD$

PROG=	more
MAN=
CFLAGS+=-D_USE_OLD_CURSES_
DPADD=	${LIBOCURSES} ${LIBCOMPAT}
LDADD=	-locurses -lcompat

.include <bsd.prog.mk>
