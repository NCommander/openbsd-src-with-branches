#	$OpenBSD: Makefile,v 1.12 2016/03/30 06:38:46 jmc Exp $

PROG=	tput
SRCS=	clear_cmd.c reset_cmd.c tparm_type.c tput.c transform.c \
	tty_settings.c
CURSES=	${.CURDIR}/../../lib/libcurses
DPADD=	${LIBCURSES}
LDADD=	-lcurses
LINKS=	${BINDIR}/tput ${BINDIR}/clear
TIC= ${.CURDIR}/../tic
CFLAGS+= -I${CURSES} -I${TIC} -I${.CURDIR} -I.
.PATH:  ${TIC}
CLEANFILES+= termsort.h

termsort.h: ${TIC}/MKtermsort.sh
	sh ${TIC}/MKtermsort.sh awk ${CURSES}/Caps > ${.TARGET}

.include <bsd.prog.mk>
