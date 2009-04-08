#	$OpenBSD: Makefile,v 1.2 2002/08/21 22:49:41 wcobb Exp $

PROG=	flock

TESTS!=jot 15 1

.for a in ${TESTS}
t-${a}:
	./flock /tmp ${a}
REGRESS_TARGETS+=t-${a}
.endfor

.include <bsd.regress.mk>
