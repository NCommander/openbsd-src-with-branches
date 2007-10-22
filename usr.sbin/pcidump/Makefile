#	$OpenBSD: Makefile,v 1.1 2007/10/22 01:12:28 dlg Exp $

.if (${MACHINE} == "amd64") || (${MACHINE} == "i386") || \
    (${MACHINE} == "macppc") || (${MACHINE} == "sparc64") || \
    (${MACHINE} == "alpha") || (${MACHINE} == "sgi") || \
    (${MACHINE} == "hppa") || (${MACHINE} == "hppa64") || \
    (${MACHINE} == "armish") || (${MACHINE} == "landisk")

PROG=	pcidump

.else
NOPROG=yes
.endif

MAN=	pcidump.8

.include <bsd.prog.mk>
