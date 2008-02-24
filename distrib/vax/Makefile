#	$OpenBSD: Makefile,v 1.3 2001/02/24 10:55:02 hugh Exp $

SUBDIR=	../ramdisk

.if	make(obj) || make(cleandir) || make(clean)
SUBDIR+= iso
.endif

.include <bsd.subdir.mk>
