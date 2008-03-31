#	$OpenBSD: Makefile,v 1.3 2007/10/13 21:22:27 deraadt Exp $

SUBDIR= ramdisk cdfs

.if	make(obj) || make(cleandir) || make(clean)
SUBDIR+= iso
.endif

.include <bsd.subdir.mk>
