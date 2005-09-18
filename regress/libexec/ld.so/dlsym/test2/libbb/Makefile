# $OpenBSD: Makefile,v 1.1.1.1 2005/09/15 22:16:27 kurt Exp $

.include <../Makefile.inc>

LIB=bb
SRCS= bb.c
LDADD=		-Wl,-E -Wl,-rpath,$(CC_OBJDIR) -L$(CC_OBJDIR) -lcc
.include <bsd.lib.mk>
