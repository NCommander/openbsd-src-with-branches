#	$OpenBSD: Makefile,v 1.1 2019/03/26 22:20:54 rob Exp $

REGRESS_TARGETS= run-regress run-regress-int-i

CFLAGS= -lutil

run-regress: ber_test
	./ber_test

run-regress-int-i: ber_test_int_i
	./ber_test_int_i

.include <bsd.regress.mk>
