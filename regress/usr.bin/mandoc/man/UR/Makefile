# $OpenBSD: Makefile,v 1.2 2014/07/07 21:35:42 schwarze Exp $

REGRESS_TARGETS	 = args noUE
LINT_TARGETS	 = args noUE

# groff-1.22.3 defects:
# - Without .UE, .UR does not print the URI.
# - But .UE prints <> even without .UR.

SKIP_GROFF	 = noUE

.include <bsd.regress.mk>
