# $OpenBSD: Makefile,v 1.2 2017/03/05 19:59:26 schwarze Exp $

REGRESS_TARGETS	 = args noname nostd
LINT_TARGETS	 = noname nostd

# groff-1.22.2 defect:
# - .Rv without -std produces no output

SKIP_GROFF	 = nostd

# ugly name section output, do not test this for now

SKIP_MARKDOWN	?= noname

.include <bsd.regress.mk>
