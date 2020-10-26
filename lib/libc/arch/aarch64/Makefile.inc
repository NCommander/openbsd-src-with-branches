# $OpenBSD: Makefile.inc,v 1.1 2017/01/11 18:09:24 patrick Exp $
# $NetBSD: Makefile.inc,v 1.5 2002/07/10 04:29:06 thorpej Exp $

.include <bsd.own.mk>

# Suppress DWARF2 warnings
DEBUG?= -gdwarf-4

CERROR=	cerror.S
