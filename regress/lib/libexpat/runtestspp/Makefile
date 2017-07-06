# $OpenBSD: Makefile,v 1.1.1.1 2017/06/30 14:56:08 bluhm Exp $

SRCS=		runtestspp.cpp chardata.c memcheck.c minicheck.c
# align_limit_to_full_utf8_characters is not exported in dynamic library
LDFLAGS=	-static

.include <bsd.regress.mk>
