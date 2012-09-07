# $OpenBSD: Makefile,v 1.1 2012/09/06 22:13:44 matthew Exp $

.include <bsd.own.mk>
.if ${COMPILER_VERSION} == "gcc2"
CFLAGS+=	-finit-priority
.endif

PROG=init_priority_test
SRCS=init_priority_test.cc

.include <bsd.regress.mk>
