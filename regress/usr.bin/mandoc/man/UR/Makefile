# $OpenBSD: Makefile,v 1.1 2013/10/17 20:51:31 schwarze Exp $

REGRESS_TARGETS	= args noUE
LINT_TARGETS	= noUE

# groff-1.22.2 defect:
# - without .UE, .UR does not print the URI.

SKIP_GROFF	= noUE

.include <bsd.regress.mk>
