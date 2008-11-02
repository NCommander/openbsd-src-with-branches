#	$OpenBSD$

LIB=		dd
SRCS=		dd.c
LDADD+=		-Wl,-E
LDADD+=		-Wl,-rpath,$(EE_OBJDIR)
LDADD+=		-L$(EE_OBJDIR) -lee

regress: all

.include <bsd.lib.mk>
