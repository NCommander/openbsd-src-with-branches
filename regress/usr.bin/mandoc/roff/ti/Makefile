# $OpenBSD: Makefile,v 1.1 2017/05/08 15:33:43 schwarze Exp $

REGRESS_TARGETS	 = basic-mdoc basic-man wide

# groff-1.22.4 defect:
# - Excessive temporare indentations cause excessive line lengths
#   instead of being truncated to resonable indentations.

SKIP_GROFF	 = wide

.include <bsd.regress.mk>
