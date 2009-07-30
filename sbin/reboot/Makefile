#	$OpenBSD: Makefile,v 1.5 1997/09/21 11:37:59 deraadt Exp $

PROG=	reboot
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	reboot.8
MLINKS=	reboot.8 halt.8
LINKS=	${BINDIR}/reboot ${BINDIR}/halt

.include <bsd.prog.mk>
