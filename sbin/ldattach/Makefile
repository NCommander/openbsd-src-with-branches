#	$OpenBSD: Makefile,v 1.1.1.1 2007/11/03 15:22:54 mbalmer Exp $

PROG=		ldattach
SRCS=		ldattach.c atomicio.c
MAN=		ldattach.8

CFLAGS+=	-Wall -Werror
LDADD+=		-lutil

BINDIR=		/sbin

.include <bsd.prog.mk>
