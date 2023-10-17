#	$OpenBSD: Makefile,v 1.12 2019/12/21 21:40:01 espie Exp $

PROG=	tset
SRCS=	reset_cmd.c transform.c tset.c tty_settings.c
CURSES=	${.CURDIR}/../../lib/libcurses
DPADD=	${LIBCURSES}
LDADD=	-lcurses
LINKS=	${BINDIR}/tset ${BINDIR}/reset
TIC= ${.CURDIR}/../tic
CFLAGS+= -I${CURSES} -I${TIC} -I${.CURDIR} -I.
.PATH:  ${TIC}
CLEANFILES+= termsort.h

.include <bsd.prog.mk>
