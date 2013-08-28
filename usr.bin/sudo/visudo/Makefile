#	$OpenBSD: Makefile,v 1.3 2008/11/14 11:58:08 millert Exp $

.PATH:		${.CURDIR}/..

PROG=	visudo
SRCS=	pwutil.c visudo.c
BINDIR=	/usr/sbin
CPPFLAGS+=	-I..

MAN=	visudo.8

CLEANFILES+= ${MAN}

visudo.8: ${.CURDIR}/../varsub visudo.mdoc.in
	sed -f ${.ALLSRC} > $@

afterdepend: ${MAN}

.include <bsd.prog.mk>
