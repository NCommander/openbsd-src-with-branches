#	$OpenBSD: Makefile,v 1.1 2007/01/28 19:10:06 kettenis Exp $

PROG=	exceptions
SRCS=	exceptions.cc

REGRESS_TARGETS=runs

runs: exceptions
	for i in $$(jot 100); do exceptions; done

.include <bsd.regress.mk>
