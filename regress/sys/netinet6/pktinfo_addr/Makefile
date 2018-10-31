# $OpenBSD: Makefile,v 1.2 2016/03/30 22:02:55 vgross Exp $

PROG	   = runtest

REGRESS_TARGETS ?=	run-regress-runtest

run-regress-runtest: ${PROG}
	${SETENV} PROG=${.OBJDIR}/${PROG} SUDO=${SUDO} /bin/ksh ${.CURDIR}/test_wrapper.sh

.include <bsd.regress.mk>
