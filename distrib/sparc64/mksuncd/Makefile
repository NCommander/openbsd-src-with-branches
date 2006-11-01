#	$OpenBSD: Makefile,v 1.2 2001/10/12 21:59:27 jason Exp $

.if ${MACHINE} == "sparc64"

PROG=mksuncd
BINDIR=/usr/bin

CFLAGS+=-Werror -Wall -Wstrict-prototypes -Wmissing-prototypes

.else
NOPROG=
.endif

.include <bsd.prog.mk>

