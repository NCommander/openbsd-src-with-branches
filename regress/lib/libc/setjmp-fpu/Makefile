#	$OpenBSD: Makefile,v 1.2 2013/12/29 05:46:43 martynas Exp $

PROGS=	_setjmp setjmp sigsetjmp

LDADD=	-lm

.for p in ${PROGS}
REGRESS_TARGETS +=	run-$p
run-$p: $p
	@echo '======== $@ ========'
	./$p
.endfor

.include <bsd.regress.mk>
