#	$OpenBSD: Makefile.in,v 1.1.1.1 2007/09/15 18:12:31 otto Exp $
#
# Makefile for the cc part of pcc.
#
PROG=   	cc
PREFIX=		/usr/local
BINDIR=		${PREFIX}/bin
MANDIR=		${PREFIX}/man1
TARGOS=		openbsd
TARGMACH=	x86

CFLAGS+=	-DLIBEXECDIR=\"${PREFIX}/libexec\"
CPPFLAGS+=	-I${.CURDIR}/../.. -I${.CURDIR}/../../os/${TARGOS}
CPPFLAGS+=	-Dos_${TARGOS} -Dmach_${TARGMACH}

.include <bsd.prog.mk>
