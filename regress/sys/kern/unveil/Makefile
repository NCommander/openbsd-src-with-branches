#	$OpenBSD: Makefile,v 1.3 2019/08/06 20:21:36 bluhm Exp $

WARNINGS=	yes

PROGS+=			syscalls
LDADD_syscalls=		-lutil
DPADD_syscalls=		${LIBUTIL}
REGRESS_ROOT_TARGETS+=	run-regress-syscalls
run-regress-syscalls:
	${SUDO} ./syscalls

PROGS+=				socket
REGRESS_EXPECTED_FAILURES+=	run-regress-socket

.include <bsd.regress.mk>
