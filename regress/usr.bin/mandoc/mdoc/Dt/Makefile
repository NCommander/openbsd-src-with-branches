# $OpenBSD: Makefile,v 1.2 2014/06/20 22:58:41 schwarze Exp $

REGRESS_TARGETS	 = case badsec badarch nodate baddate
REGRESS_TARGETS	+= noos order dupe nobody lateos
LINT_TARGETS	 = case badsec badarch nodate baddate
LINT_TARGETS	+= noos order dupe nobody lateos

SKIP_GROFF	 = badsec badarch nodate baddate noos nobody
SKIP_ASCII	 = badsec badarch nodate baddate noos

.include <bsd.regress.mk>
