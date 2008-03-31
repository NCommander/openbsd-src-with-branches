#	$OpenBSD: Makefile,v 1.2 2002/08/21 22:49:41 wcobb Exp $

PROG=misc
CLEANFILES=mapped_file

REGRESS_ROOT_TARGETS=	run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	${SUDO} ./${PROG}

.include <bsd.regress.mk>
