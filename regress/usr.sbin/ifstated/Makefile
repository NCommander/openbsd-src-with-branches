# $OpenBSD: Makefile,v 1.3 2018/02/05 18:28:15 anton Exp $
# Regress tests for ifstated

REGRESS_TARGETS+=	statemachine

REGRESS_TARGETS+=	ifstated
REGRESS_ROOT_TARGETS+=	ifstated

statemachine:
	env OBJDIR=${.OBJDIR} sh ${.CURDIR}/statemachine

ifstated:
	${SUDO} env OBJDIR=${.OBJDIR} sh ${.CURDIR}/ifstated

.include <bsd.regress.mk>
