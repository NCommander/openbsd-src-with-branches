# $OpenBSD: Makefile,v 1.2 2010/12/09 23:01:18 schwarze Exp $

REGRESS_TARGETS=escape infinite zerolength
GROFF_TARGETS=escape zerolength

# The infinite test fails badly with groff-1.20.1:
# It fails to print the following text.

.include <bsd.regress.mk>
