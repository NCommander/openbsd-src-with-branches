# $OpenBSD$

PROG=		prog2
SRCS=		main.c
LDFLAGS+=	-Wl,-E
LDFLAGS+=	-Wl,-rpath,$(AA_OBJDIR)
LDFLAGS+=	-Wl,-rpath,$(BB_OBJDIR)

.include <bsd.regress.mk>
