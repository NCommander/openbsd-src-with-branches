# $OpenBSD: Makefile,v 1.2 2011/02/05 18:19:11 schwarze Exp $

REGRESS_TARGETS=section noarg

# groff 1.21 does not handle .nr nS

SKIP_GROFF ?= section

SKIP_TMAN ?= ALL

.include <bsd.regress.mk>
