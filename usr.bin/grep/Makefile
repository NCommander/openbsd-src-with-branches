#	$OpenBSD$

PROG=	grep
SRCS=	grep.c
CFLAGS+= -Wall -pedantic -g

LINKS+=	${BINDIR}/grep ${BINDIR}/egrep \
	${BINDIR}/grep ${BINDIR}/fgrep
MLINKS=	grep.1 egrep.1 grep.1 fgrep.1

.include <bsd.prog.mk>
