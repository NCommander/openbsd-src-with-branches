# $OpenBSD$

REGRESSTARGETS=shcrash.sh

shcrash.sh:
	ulimit -c 0 && sh ${.CURDIR}/shcrash.sh

.PHONY: ${REGRESSTARGETS}

.include <bsd.regress.mk>
