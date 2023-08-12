# $OpenBSD$

.include <bsd.own.mk>

REGRESS_TARGETS = test

.include <bsd.regress.mk>

CLEANFILES+=empty short

.PHONY: test
test: empty short
	${SHELL} ${.CURDIR}/test.sh

empty:
	touch empty

short:
	dd if=/usr/bin/ldd of=short count=128 bs=1
