#	$OpenBSD$

PROG=	mille
SRCS=	comp.c end.c extern.c init.c mille.c misc.c move.c print.c \
	roll.c save.c types.c varpush.c
DPADD=	${LIBCURSES} ${LIBTERMLIB} ${LIBCOMPAT}
LDADD=	-lcurses -ltermlib -lcompat
MAN=	mille.6
HIDEGAME=hidegame

.include <bsd.prog.mk>

