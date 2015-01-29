# $OpenBSD: Makefile,v 1.12 2015/01/28 17:30:37 schwarze Exp $

REGRESS_TARGETS	 = man nested
LINT_TARGETS	 = man nested

# trivial difference to groff-1.22.3:
# .TS in a table causes a blank table line in GNU tbl(1), but not in mandoc.

SKIP_GROFF	 = nested

.include <bsd.regress.mk>
