#	$OpenBSD: Makefile,v 1.6 1998/11/16 03:08:40 millert Exp $

PROG=	infocmp
SRCS=	infocmp.c dump_entry.c
DPADD=	${LIBCURSES}
LDADD=	-lcurses
CURSES= ${.CURDIR}/../../lib/libcurses
TIC= ${.CURDIR}/../tic
CFLAGS+= -I${CURSES} -I${TIC} -I${.CURDIR} -I.
.PATH:  ${TIC}

beforedepend: termsort.c

termsort.c: ${TIC}/MKtermsort.sh
	sh ${TIC}/MKtermsort.sh awk ${CURSES}/Caps > ${.TARGET}

.include <bsd.prog.mk>
