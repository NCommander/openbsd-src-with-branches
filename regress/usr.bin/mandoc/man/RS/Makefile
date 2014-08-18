# $OpenBSD: Makefile,v 1.7 2014/07/07 21:35:42 schwarze Exp $

REGRESS_TARGETS	 = breaking broken empty literal lonelyRE nested noRE width
LINT_TARGETS	 = broken lonelyRE noRE

# groff-1.22.2/mandoc difference:
# RS PP RE gives a blank line in groff, none in mandoc.

SKIP_GROFF	 = broken

.include <bsd.regress.mk>
