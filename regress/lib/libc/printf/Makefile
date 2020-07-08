#	$OpenBSD: Makefile,v 1.1 2008/09/07 20:36:10 martynas Exp $

PROGS =			fp string

REGRESS_TARGETS =	${PROGS:S/^/run-regress-/}

.for p in ${PROGS}
run-regress-${p}: ${p}
	./${p}
.PHONY: run-regress-${p}
.endfor

.include <bsd.regress.mk>
