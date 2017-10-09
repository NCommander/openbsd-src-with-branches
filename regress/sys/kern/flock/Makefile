#	$OpenBSD: Makefile,v 1.2 2010/06/20 14:14:49 phessler Exp $

PROG=	flock

TESTS!=jot 15 1

# XXX known failures (talk to art)
REGRESS_SKIP_TARGETS = t-5 t-6

.for a in ${TESTS}
t-${a}: flock
	./flock /tmp ${a}
REGRESS_TARGETS+=t-${a}
.endfor

.include <bsd.regress.mk>
