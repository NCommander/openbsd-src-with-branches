#	$OpenBSD: Makefile,v 1.3 2007/10/07 17:58:50 otto Exp $
#
# Makefile for the cc part of pcc.
#
PROG=   	cc
PREFIX=		/usr/local
BINDIR=		${PREFIX}/bin
MANDIR=		${PREFIX}/man/man
TARGOS=		openbsd

.if ${MACHINE_ARCH} == "i386"
TARGMACH=       x86
.else
TARGMACH=       ${MACHINE_ARCH}
.endif


CFLAGS+=	-DLIBEXECDIR=\"${PREFIX}/libexec\"
CPPFLAGS+=	-I${.CURDIR}/..
CPPFLAGS+=	-DDEFMACH=\"${TARGMACH}\"

.include <bsd.prog.mk>
