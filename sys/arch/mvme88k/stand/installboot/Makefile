#	$OpenBSD: Makefile,v 1.4 1999/09/27 19:30:00 smurph Exp $

.if ${MACHINE} == "mvme88k"
PROG=	installboot
MAN=	installboot.8
MANSUBDIR=/mvme88k

BINDIR=/usr/mdec
DPADD=${LIBUTIL}
LDADD=-lutil
CFLAGS+=-O0
# Need this to work in the miniroot
LDSTATIC=-static
.else
NOPROG=
.endif

.include <bsd.prog.mk>

