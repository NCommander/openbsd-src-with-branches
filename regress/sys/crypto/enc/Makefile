#       $OpenBSD: Makefile,v 1.3 2002/10/21 20:54:02 mickey Exp $

PROG=   des3
LDADD=-ldes
DPADD=${LIBDES}

REGRESS_ROOT_TARGETS=	run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	${SUDO} ./${PROG}

.include <bsd.regress.mk>
