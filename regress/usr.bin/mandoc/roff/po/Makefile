# $OpenBSD: Makefile,v 1.1 2017/06/14 22:50:37 schwarze Exp $

REGRESS_TARGETS	= basic-mdoc range

# groff-1.22.4 defect:
# - Negative page objects sometimes result in output lines starting
#   with backspace characters.
# - Excessive page offsets cause excessive line lengths
#   instead of being truncated to resonable widths.

SKIP_GROFF	 = range

.include <bsd.regress.mk>
