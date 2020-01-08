# $OpenBSD: Makefile,v 1.3 2015/01/30 21:28:21 schwarze Exp $

REGRESS_TARGETS	= break vspace
LINT_TARGETS	= break
GOPTS		= -t

# groff-1.22.3 defect:
# - Starting a table in next-line scope confuses font handling,
#   and in the case of .TP indentation as well.

SKIP_GROFF	= break

.include <bsd.regress.mk>
