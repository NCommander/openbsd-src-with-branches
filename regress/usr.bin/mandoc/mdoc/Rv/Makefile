# $OpenBSD$

REGRESS_TARGETS	 = args noname nostd
LINT_TARGETS	 = noname nostd

# groff-1.22.2 defect:
# - .Rv without -std produces no output

SKIP_GROFF	 = nostd

.include <bsd.regress.mk>
