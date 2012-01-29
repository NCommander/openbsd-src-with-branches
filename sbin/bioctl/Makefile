#	$OpenBSD: Makefile,v 1.9 2008/06/13 21:03:40 hshoexer Exp $

PROG=	bioctl
SRCS=	bioctl.c pbkdf2.c

LDADD=	-lutil
DPADD=	${LIBUTIL}

CDIAGFLAGS=     -Wall
CDIAGFLAGS+=    -Wpointer-arith
CDIAGFLAGS+=    -Wno-uninitialized
CDIAGFLAGS+=    -Wstrict-prototypes
CDIAGFLAGS+=    -Wmissing-prototypes
CDIAGFLAGS+=    -Wunused
CDIAGFLAGS+=    -Wsign-compare
CDIAGFLAGS+=    -Wbounded
CDIAGFLAGS+=    -Wshadow

MAN=	bioctl.8

.include <bsd.prog.mk>
