#	$OpenBSD: Makefile,v 1.4 2007/10/14 20:24:00 otto Exp $
#
# Makefile for the cc part of pcc.
#
PROG=   	cc
PREFIX=		/usr/local
BINDIR=		${PREFIX}/bin
MANDIR=		${PREFIX}/man/man
TARGOS=		openbsd
TARGMACH=       ${MACHINE_ARCH}

CFLAGS+=	-DLIBEXECDIR=\"${PREFIX}/libexec\"
CPPFLAGS+=	-I${.CURDIR}/..
CPPFLAGS+=	-DDEFMACH=\"${TARGMACH}\"

.include <bsd.prog.mk>
