#	$OpenBSD: Makefile,v 1.3 2020/12/03 19:16:57 anton Exp $

PROG=		vnode

SRCS+=		vnode.c
SRCS+=		test-deadlock.c

WARNINGS=	yes

TESTS+=	deadlock

REGRESS_SETUP=	${PROG}

.for t in ${TESTS}
run-$t:
	f=`mktemp`; trap 'rm $$f' EXIT; ./${PROG} -f $$f $t
REGRESS_TARGETS+=	run-$t
.endfor

.include <bsd.regress.mk>
