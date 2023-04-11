# $OpenBSD: Makefile,v 1.3 2021/10/11 05:46:14 anton Exp $
# Regress tests for snmpd

REGRESS_SETUP_ONCE =	setup
REGRESS_CLEANUP =	cleanup
REGRESS_TARGETS =	run-snmpd

setup:
	${SUDO} /etc/rc.d/snmpd stop

cleanup:
	-${SUDO} /etc/rc.d/snmpd restart || true

run-snmpd:
	${SUDO} env OBJDIR=${.OBJDIR} sh ${.CURDIR}/snmpd.sh

.include <bsd.regress.mk>
