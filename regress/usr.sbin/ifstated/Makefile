# $OpenBSD$
# Regress tests for ifstated

REGRESS_TARGETS =	run-regress-statemachine run-regress-ifstated

run-regress-statemachine:
	sh ${.CURDIR}/statemachine

run-regress-ifstated:
	sh ${.CURDIR}/ifstated

.include <bsd.regress.mk>
