#	$OpenBSD: Makefile,v 1.3 1994/12/07 05:08:04 jtc Exp $
#	$NetBSD: Makefile,v 1.3 1994/12/07 05:08:04 jtc Exp $

PROG=	tset
SRCS=	map.c misc.c set.c term.c tset.c wrterm.c

DPADD=	${LIBTERMCAP}
LDADD=	-ltermcap
LINKS=	${BINDIR}/tset ${BINDIR}/reset
MLINKS=	tset.1 reset.1

.include <bsd.prog.mk>
