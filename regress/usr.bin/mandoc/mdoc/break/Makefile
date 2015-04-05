# $OpenBSD: Makefile,v 1.2 2015/02/12 13:52:12 schwarze Exp $

REGRESS_TARGETS	= brokenbreaker twice tail two
LINT_TARGETS	= brokenbreaker twice tail two

# It's hard to keep stuff together in next-line scope.

SKIP_TMAN	= tail

# groff-1.22.3 defect:
# - non-matching enclosure end macro prints a closing delimiter

SKIP_GROFF	= brokenbreaker

.include <bsd.regress.mk>
