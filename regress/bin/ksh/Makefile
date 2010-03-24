# $OpenBSD: Makefile,v 1.3 2003/02/09 18:52:49 espie Exp $

REGRESS_TARGETS=shcrash.sh seterror.sh varfunction.sh

shcrash.sh:
	ulimit -c 0 && sh ${.CURDIR}/shcrash.sh

seterror.sh:
	sh ${.CURDIR}/seterror.sh

varfunction.sh:
	sh ${.CURDIR}/varfunction.sh

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
