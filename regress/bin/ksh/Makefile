# $OpenBSD: Makefile,v 1.1 2002/02/27 19:57:17 dhartmei Exp $

REGRESS_TARGETS=shcrash.sh

shcrash.sh:
	ulimit -c 0 && sh ${.CURDIR}/shcrash.sh

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
