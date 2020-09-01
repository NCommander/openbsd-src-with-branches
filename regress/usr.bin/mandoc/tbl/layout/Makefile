# $OpenBSD: Makefile,v 1.6 2020/01/11 20:56:26 schwarze Exp $

REGRESS_TARGETS	 = badspan center complex empty emptycol emptyline
REGRESS_TARGETS	+= lines lines-nogroff numbers
REGRESS_TARGETS	+= shortlines spacing spacing-nogroff span
LINT_TARGETS	 = badspan complex empty spacing-nogroff

# groff-1.22.4 defects:
# - When the layout is completely empty,
#   GNU eqn produces no output whatsoever for the whole table.
# - When there is no horizontal line at the top,
#   vertical lines extend beyond the top of the table.
# - Excessive spacing modifiers in the layout are honoured.

SKIP_GROFF	 = empty lines-nogroff spacing-nogroff

.include <bsd.regress.mk>
