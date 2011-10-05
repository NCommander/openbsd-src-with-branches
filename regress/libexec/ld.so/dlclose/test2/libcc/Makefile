#	$OpenBSD: Makefile,v 1.1.1.1 2005/09/28 15:42:32 kurt Exp $

LIB=		cc
SRCS=		cc.c
LDADD+=		-Wl,-E
LDADD+=		-Wl,-rpath,$(ZZ_OBJDIR)
LDADD+=		-L$(ZZ_OBJDIR) -lzz

regress: all

.include <bsd.lib.mk>
