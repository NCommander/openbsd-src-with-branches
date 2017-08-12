# $OpenBSD: Makefile,v 1.1 2017/07/14 10:41:30 benno Exp $
# Regress tests for ifstated

REGRESS_TARGETS =	run-regress-statemachine run-regress-ifstated

run-regress-statemachine:
	env OBJDIR=${.OBJDIR} ${SUDO} sh ${.CURDIR}/statemachine

run-regress-ifstated:
	env OBJDIR=${.OBJDIR} ${SUDO} sh ${.CURDIR}/ifstated

.include <bsd.regress.mk>
