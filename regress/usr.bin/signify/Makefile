#	$OpenBSD: Makefile,v 1.1 2014/01/09 16:13:44 tedu Exp $

CLEANFILES += test.sig
REGRESS_TARGETS = t1

t1:
	@sh ${.CURDIR}/signify.sh ${.CURDIR}

.PHONY: t1

.include <bsd.regress.mk>
