#	$OpenBSD: Makefile,v 1.4 2011/07/07 21:00:59 deraadt Exp $

PROG =		gaitest
CLEANFILES =	out

run-regress-${PROG}: ${PROG}
	sh ${.CURDIR}/testsuite.sh >out 2>&1
	diff -u10 out ${.CURDIR}/answer

.include <bsd.regress.mk>
