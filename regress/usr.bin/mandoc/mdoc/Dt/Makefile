# $OpenBSD: Makefile,v 1.1 2014/06/20 18:32:51 schwarze Exp $

REGRESS_TARGETS	 = case badsec badarch nodate baddate
REGRESS_TARGETS	+= noos order dupe nobody lateos befSh
LINT_TARGETS	 = case badsec badarch nodate baddate
LINT_TARGETS	+= noos order dupe nobody lateos befSh

SKIP_GROFF = badsec badarch nodate baddate noos nobody befSh
SKIP_ASCII = badsec badarch nodate baddate noos befSh

.include <bsd.regress.mk>
