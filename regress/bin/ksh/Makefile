# $OpenBSD: Makefile,v 1.4 2003/12/15 05:28:40 otto Exp $

REGRESS_TARGETS=shcrash.sh seterror.sh varfunction.sh eval.sh

shcrash.sh:
	ulimit -c 0 && sh ${.CURDIR}/shcrash.sh

seterror.sh:
	sh ${.CURDIR}/seterror.sh

varfunction.sh:
	sh ${.CURDIR}/varfunction.sh

eval.sh:
	test `sh ${.CURDIR}/eval.sh | wc -l` == 4

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
