#	$OpenBSD: Makefile,v 1.6 2003/06/25 15:00:04 millert Exp $

PROG=	grep
SRCS=	binary.c file.c grep.c mmfile.c queue.c util.c
LINKS=	${BINDIR}/grep ${BINDIR}/egrep \
	${BINDIR}/grep ${BINDIR}/fgrep \
	${BINDIR}/grep ${BINDIR}/zgrep \
	${BINDIR}/grep ${BINDIR}/zegrep \
	${BINDIR}/grep ${BINDIR}/zfgrep \

CFLAGS+= -Wall

LDADD=	-lz
DPADD=	${LIBZ}

.include <bsd.prog.mk>
