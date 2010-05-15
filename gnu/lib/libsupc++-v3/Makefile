# $OpenBSD: Makefile,v 1.1 2010/05/15 08:23:59 robert Exp $

.include "${.CURDIR}/../libstdc++-v3/Makefile.common" 

# Static only.
LIB=	supc++

# Pulled from Makefile.common
SRCS=	${SUPSRCS}

# Build a standard .a lib with PIC objects
NOPIC=
CFLAGS+= ${PICFLAG}

.include <bsd.lib.mk>
