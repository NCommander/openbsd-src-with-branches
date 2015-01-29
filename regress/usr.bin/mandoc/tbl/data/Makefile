# $OpenBSD: Makefile,v 1.12 2015/01/28 17:30:37 schwarze Exp $

REGRESS_TARGETS	 = blankline block_unclosed empty
LINT_TARGETS	 = block_unclosed empty

# groff-1.22.3 defect:
# - When a table ends in the middle of a block,
#   GNU eqn produces no output whatsoever for the whole table.

SKIP_GROFF	 = block_unclosed

.include <bsd.regress.mk>
