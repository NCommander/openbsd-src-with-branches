#	$OpenBSD$
#	$NetBSD: Makefile,v 1.2 1997/01/22 01:36:30 cgd Exp $

S=${.CURDIR}/../../../..
ZDST=${.OBJDIR}
SADIR=${.CURDIR}/..

.PATH: ${S}/lib/libz

.include "${S}/lib/libz/Makefile"
CFLAGS+=${SACFLAGS} -I${S}/lib/libsa ${DEBUGFLAGS}
