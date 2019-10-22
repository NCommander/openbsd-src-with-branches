# $OpenBSD: Makefile,v 1.4 2015/05/14 02:56:01 jsg Exp $

.include "${.CURDIR}/../libstdc++-v3/Makefile.common" 

# Static only.
LIB=	supc++

# Pulled from Makefile.common
SRCS=	${SUPSRCS}

# Build a standard .a lib with PIC objects
NOPIC=
CFLAGS+= ${PICFLAG}
CXXFLAGS+= ${PICFLAG}

.include <bsd.lib.mk>
