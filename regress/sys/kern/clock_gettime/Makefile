# $OpenBSD$

PROG=	monotonicrelapse
LDADD+=	-lpthread
DPADD+=	${LIBPTHREAD}
WARNINGS=	yes

NCPUONLINE!=	sysctl -n hw.ncpuonline

REGRESS_TARGETS+=	run-regress-monotonicrelapse

run-regress-monotonicrelapse: ${PROG}
	${PROG} ${NCPUONLINE}

.include <bsd.regress.mk>
