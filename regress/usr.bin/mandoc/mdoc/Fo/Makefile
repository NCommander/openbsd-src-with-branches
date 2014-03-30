# $OpenBSD: Makefile,v 1.6 2012/11/18 00:05:28 schwarze Exp $

REGRESS_TARGETS = basic section break noarg font eos

# groff does not handle .nr nS

SKIP_GROFF ?= section

SKIP_TMAN = eos

.include <bsd.regress.mk>
