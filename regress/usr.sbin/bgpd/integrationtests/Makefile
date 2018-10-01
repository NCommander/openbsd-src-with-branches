# $OpenBSD: Makefile,v 1.6 2018/09/21 05:14:07 claudio Exp $

REGRESS_TARGETS = network_statement

network_statement:
	${SUDO} ksh ${.CURDIR}/$@.sh

.include <bsd.regress.mk>
