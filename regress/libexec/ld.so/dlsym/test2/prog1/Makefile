# $OpenBSD: Makefile,v 1.2 2005/09/18 11:56:34 kurt Exp $

PROG=		prog1
SRCS=		main.c
LDADD+=		-laa
LDFLAGS+=	-Wl,-E
LDFLAGS+=	-Wl,-rpath,$(AA_OBJDIR)
LDFLAGS+=	-Wl,-rpath,$(BB_OBJDIR)
LDFLAGS+=	-L$(AA_OBJDIR)

.include <bsd.regress.mk>
