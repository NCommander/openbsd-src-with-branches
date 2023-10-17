#	$OpenBSD: Makefile,v 1.11 2022/07/11 03:11:49 daniel Exp $

PROG=	tic
SRCS=	dump_entry.c tic.c tparm_type.c transform.c
CURSES=	${.CURDIR}/../../lib/libcurses
DPADD=	${LIBCURSES}
LDADD=	 -lcurses
LINKS=	${BINDIR}/tic ${BINDIR}/captoinfo ${BINDIR}/tic ${BINDIR}/infotocap
CFLAGS+= -I${CURSES} -I${.CURDIR} -I.
CLEANFILES+= termsort.h

dump_entry.o: termsort.h

termsort.h: ${.CURDIR}/MKtermsort.sh
	sh ${.CURDIR}/MKtermsort.sh awk ${CURSES}/Caps > ${.TARGET}

.include <bsd.prog.mk>
