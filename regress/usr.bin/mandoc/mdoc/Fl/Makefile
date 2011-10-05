# $OpenBSD: Makefile,v 1.3 2010/07/01 05:29:57 schwarze Exp $

REGRESS_TARGETS=noarg multiarg parsed punct
GROFF_TARGETS=noarg multiarg parsed

# Do not regenerate the punct output file for now,
# because groff-1.15 and groff-1.20.1 handle
# the bar differently as a .Fl argument.

.include <bsd.regress.mk>
