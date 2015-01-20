#	$OpenBSD: Makefile,v 1.1 2014/04/30 05:32:00 djm Exp $

LIB=	test_helper
SRCS=	test_helper.c fuzz.c

DEBUGLIBS= no
NOPROFILE= yes
NOPIC=	yes

# Hack to allow building with SUBDIR in ../../Makefile
regress: all

install:
	@echo -n

.include <bsd.lib.mk>
