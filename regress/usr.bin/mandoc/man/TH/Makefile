# $OpenBSD: Makefile,v 1.5 2014/06/20 22:58:41 schwarze Exp $

REGRESS_TARGETS  = noTH noarg onearg twoargs sixargs case
REGRESS_TARGETS += emptydate onlyyear isodate mdocdate baddate
REGRESS_TARGETS += nobody

LINT_TARGETS = noTH case twoargs emptydate baddate nobody

SKIP_GROFF = noTH
SKIP_ASCII = noTH

.include <bsd.regress.mk>
