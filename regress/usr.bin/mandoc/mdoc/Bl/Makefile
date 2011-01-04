# $OpenBSD: Makefile,v 1.5 2010/07/01 05:29:57 schwarze Exp $

REGRESS_TARGETS=tag inset hang diag column extend nested
GROFF_TARGETS=tag inset diag column extend nested

# mandoc still contains a compatibility hack with groff-1.15
# regarding hang list indentation.

REGRESS_TARGETS+=multitype multitag empty
GROFF_TARGETS+=multitype multitag

# groff-1.20.1 mishandles empty lists,
# ruining indentation and sometimes cauisng empty lines.

REGRESS_TARGETS+=EMULTILIST
REGRESS_SLOW_TARGETS=EMULTILIST

.include <bsd.regress.mk>
