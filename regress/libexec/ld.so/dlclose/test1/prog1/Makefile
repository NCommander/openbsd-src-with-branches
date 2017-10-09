# $OpenBSD$

PROG=		prog1
SRCS=		main.c
LDFLAGS+=	-Wl,-E
LDFLAGS+=	-Wl,-rpath,$(AA_OBJDIR)

.include <bsd.regress.mk>
