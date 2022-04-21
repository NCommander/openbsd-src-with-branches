# $OpenBSD: Makefile,v 1.1 2011/07/09 03:58:12 nicm Exp $

REGRESS_TARGETS=	run_test

run_test:
	test=/bin/test /bin/sh $(.CURDIR)/TEST.sh >/dev/null

.include <bsd.regress.mk>
