#	$OpenBSD: Makefile,v 1.5 2012/08/21 14:46:18 pascal Exp $

S=${.CURDIR}/../../../..
ZDST=${.OBJDIR}

NOPIE=

.PATH: ${S}/lib/libz

CPPFLAGS+=	${SA_CPPFLAGS}

CFLAGS+=-I${S}

.include "${S}/lib/libz/Makefile"
