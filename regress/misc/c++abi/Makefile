# $OpenBSD$

REGRESS_TARGETS = check1

check1: nm1.out
	diff -I 'OpenBSD' -u nm1.out ${.CURDIR}/nm1.ref

nm1.out: nm1.o
	nm -C nm1.o | fgrep witness | sort | sed -e 's/.* T witness_//' >$@

CLEANFILES = *.o *.out

.include <bsd.regress.mk>
