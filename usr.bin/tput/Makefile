#	$OpenBSD: Makefile,v 1.3 1996/12/08 14:32:35 downsj Exp $
#	$NetBSD: Makefile,v 1.3 1994/12/07 08:49:07 jtc Exp $

PROG=	tput
DPADD=	${LIBTERMCAP}
LDADD=	-ltermcap
MLINKS=	tput.1 clear.1
LINKS= ${BINDIR}/tput ${BINDIR}/clear

.include <bsd.prog.mk>
