#	$OpenBSD: Makefile,v 1.3 1999/09/26 18:05:32 smurph Exp $

MAN=	installboot.8
MANSUBDIR=/mvme68k

.if ${MACHINE} == "mvme68k"
PROG=	installboot
BINDIR=/usr/mdec
DPADD=${LIBUTIL}
LDADD=-lutil
LDSTATIC=-static
.else
NOPROG=
.endif

.include <bsd.prog.mk>
