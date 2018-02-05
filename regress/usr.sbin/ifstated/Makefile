# $OpenBSD: Makefile,v 1.2 2017/08/12 21:41:16 rob Exp $
# Regress tests for ifstated

REGRESS_TARGETS =	run-regress-statemachine run-regress-ifstated

run-regress-statemachine:
	${SUDO} env OBJDIR=${.OBJDIR} sh ${.CURDIR}/statemachine

run-regress-ifstated:
	${SUDO} env OBJDIR=${.OBJDIR} sh ${.CURDIR}/ifstated

.include <bsd.regress.mk>
