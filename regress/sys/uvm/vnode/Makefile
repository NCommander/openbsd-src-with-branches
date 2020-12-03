#	$OpenBSD: Makefile,v 1.2 2020/11/05 18:25:39 anton Exp $

PROG=		vnode

SRCS+=		vnode.c
SRCS+=		test-deadlock.c

WARNINGS=	yes

TESTS+=	deadlock

REGRESS_SETUP_ONCE=	setup
setup: ${PROG}

.for t in ${TESTS}
${t}:
	@echo "\n======== ${@} ========"
	f=`mktemp`; trap 'rm $$f' EXIT; ./${PROG} -f $$f ${t}
REGRESS_TARGETS+=	${t}
.endfor

.include <bsd.regress.mk>
