#	$OpenBSD$

PROG=		prog4
SRCS=		main.c
LDADD+=		-laa
LDFLAGS+=	-Wl,-E
LDFLAGS+=	-Wl,-rpath,$(AA_OBJDIR)
LDFLAGS+=	-Wl,-rpath,$(BB_OBJDIR)
LDFLAGS+=	-Wl,-rpath,$(CC_OBJDIR)
LDFLAGS+=	-L$(AA_OBJDIR)

.include <bsd.regress.mk>
