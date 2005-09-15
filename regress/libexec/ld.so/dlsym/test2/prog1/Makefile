# $OpenBSD$

.include <../Makefile.inc>

PROG=		prog1
SRCS=		main.c
LDADD=		-Wl,-E -Wl,-rpath,$(AA_OBJDIR) -L$(AA_OBJDIR) -laa

.include <bsd.regress.mk>
