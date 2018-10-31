# $OpenBSD: Makefile,v 1.1 2018/10/01 20:58:59 benno Exp $

REGRESS_TARGETS	= 	network_statement
BGPD ?=			/usr/sbin/bgpd

network_statement:
	${SUDO} ksh -x ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11 12 pair11 pair12

.include <bsd.regress.mk>
