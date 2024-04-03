# $OpenBSD: Makefile,v 1.1 2022/04/27 17:04:15 schwarze Exp $

REGRESS_TARGETS	 = basic
LINT_TARGETS	 = basic

.include <bsd.regress.mk>

# temporary hack while working towards an update of the groff port
MANDOC = mandoc -O indent=5
GROFF = /co/groff/build/test-groff ${GOPTS} -d MF=R ${MOPTS} -Wall -P-c
