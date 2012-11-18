# $OpenBSD: Makefile,v 1.5 2012/07/09 17:52:09 schwarze Exp $

REGRESS_TARGETS = basic section noarg font eos

# groff 1.21 does not handle .nr nS

SKIP_GROFF ?= section

SKIP_TMAN = eos

.include <bsd.regress.mk>
