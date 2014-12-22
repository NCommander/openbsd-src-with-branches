# $OpenBSD: Makefile,v 1.5 2014/07/06 19:08:57 schwarze Exp $

REGRESS_TARGETS	= twice
LINT_TARGETS	= twice

# groff-1.22.3 defect:
# - non-matching enclosure end macro prints a closing delimiter

SKIP_GROFF	= twice

.include <bsd.regress.mk>
