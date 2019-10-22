# $OpenBSD: Makefile,v 1.1 2019/06/14 19:23:53 cheloha Exp $

PROG=	monotonicrelapse
LDADD+=	-lpthread
DPADD+=	${LIBPTHREAD}
WARNINGS=	yes

.if ! (make(clean) || make(cleandir) || make(obj))
NCPUONLINE!=	sysctl -n hw.ncpuonline
.endif

REGRESS_TARGETS+=	run-regress-monotonicrelapse

run-regress-monotonicrelapse: ${PROG}
	./${PROG} ${NCPUONLINE}

.include <bsd.regress.mk>
