#	$OpenBSD: Makefile,v 1.1 1996/06/02 11:43:46 deraadt Exp $

SUBDIR=	${MACHINE}

# miniroot and ramdisk are supposed to be "generic" using Makefile.inc
# to set up architectural dependencies..
.if	(${MACHINE} == "sparc")
SUBDIR+=        ramdisk miniroot
.endif

.include <bsd.subdir.mk>
