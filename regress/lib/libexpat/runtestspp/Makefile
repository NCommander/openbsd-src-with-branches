# $OpenBSD: Makefile,v 1.3 2017/11/03 18:30:34 bluhm Exp $

SRCS=		runtestspp.cpp chardata.c memcheck.c minicheck.c structdata.c
# _INTERNAL_trim_to_complete_utf8_characters is not exported in dynamic library
LDFLAGS=	-static

.include <bsd.regress.mk>
