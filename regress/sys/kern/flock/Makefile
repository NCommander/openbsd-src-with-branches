#	$OpenBSD: Makefile,v 1.10 2020/03/11 17:27:55 anton Exp $

PROGS+=		flock
SRCS_flock=	flock.c util.c

PROGS+=		lockf
SRCS_lockf=	lockf.c util.c

WARNINGS=	yes

TESTS_FLOCK!=	jot 24 1
.for t in ${TESTS_FLOCK}
run-flock-$t: flock
	./flock ${LOCKFLAGS} $t
REGRESS_TARGETS+=run-flock-$t
.endfor

TESTS_LOCKF!=	jot 2 1
.for t in ${TESTS_LOCKF}
run-lockf-$t: lockf
	./lockf ${LOCKFLAGS} $t
REGRESS_TARGETS+=run-lockf-$t
.endfor

.include <bsd.regress.mk>
