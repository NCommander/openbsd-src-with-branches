# $OpenBSD: Makefile,v 1.2 2014/08/18 16:26:13 schwarze Exp $

REGRESS_TARGETS	= broken broken_eline empty_before noarg
LINT_TARGETS	= broken broken_eline empty_before noarg

# groff-1.22.3 defects:
# - .SH without args just before EOF causes two additional blank lines.
# - Empty .SH heads cause additional vertical spacing.
# - If .TP or .IP breaks .SH, the tag becomes bold.
# - If .HP breaks .SH, a line is broken after the next input line.
# - If .RS/RE/nf/fi breaks .SH, the next input line becomes bold.
# - .UR does not break .SH.

SKIP_GROFF	= broken broken_eline noarg

.include <bsd.regress.mk>
