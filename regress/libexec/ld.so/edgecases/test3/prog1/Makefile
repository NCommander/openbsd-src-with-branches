#	$OpenBSD: Makefile,v 1.1 2005/09/19 21:50:27 drahn Exp $

PROG=		prog1
SRCS=		main.c
LDFLAGS+=	-Wl,-E
LDFLAGS+=	-Wl,-rpath,$(AA_OBJDIR)
LDFLAGS+=	-Wl,-rpath,$(BB_OBJDIR)

.include <bsd.regress.mk>
