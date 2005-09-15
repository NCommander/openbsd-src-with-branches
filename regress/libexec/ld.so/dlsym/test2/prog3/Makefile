# $OpenBSD$

.include <../Makefile.inc>

PROG=		prog3
SRCS=		main.c
LDADD=		-Wl,-E -Wl,-rpath,$(AA_OBJDIR) -L$(AA_OBJDIR) -laa

.include <bsd.regress.mk>
