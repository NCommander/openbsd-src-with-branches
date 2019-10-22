# $OpenBSD: Makefile,v 1.1 2017/08/11 17:45:02 rob Exp $
# Regress tests for snmpd

REGRESS_TARGETS =	run-regress-snmpd

run-regress-snmpd:
	env OBJDIR=${.OBJDIR} ${SUDO} sh ${.CURDIR}/snmpd.sh

.include <bsd.regress.mk>
