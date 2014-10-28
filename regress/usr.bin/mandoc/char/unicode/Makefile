# $OpenBSD: Makefile,v 1.1 2013/11/10 19:26:31 schwarze Exp $

REGRESS_TARGETS = ascii latin1 latin1diff man mdoc named namediff
SKIP_ASCII = man mdoc
UTF8_TARGETS = ascii latin1 latin1diff man mdoc named namediff
HTML_TARGETS = ascii latin1 latin1diff named namediff

SKIP_GROFF_ASCII = latin1diff namediff

.include <bsd.regress.mk>
