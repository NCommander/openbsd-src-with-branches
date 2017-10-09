#	$OpenBSD$

LIB=		bb
SRCS=		bb.c
LDADD+=		-Wl,-E
LDADD+=		-Wl,-rpath,$(CC_OBJDIR)
LDADD+=		-L$(CC_OBJDIR) -lcc

regress: all

.include <bsd.lib.mk>
