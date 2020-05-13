#	$OpenBSD: Makefile,v 1.3 2020/01/13 14:58:38 bluhm Exp $

PROGS=	fpu _setjmp setjmp sigsetjmp

LDADD=	-lm

REGRESS_TARGETS =	run-fpu
run-fpu: fpu
	@echo '======== $@ ========'
	# check fpu functions without setjmp/longjmp before testing the latter
	./fpu

.for p in ${PROGS:M*jmp}
REGRESS_TARGETS +=	run-$p
run-$p: $p
	@echo '======== $@ ========'
	./$p
.endfor

.include <bsd.regress.mk>
