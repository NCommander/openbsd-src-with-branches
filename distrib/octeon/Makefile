#	$OpenBSD: Makefile,v 1.5 2019/08/04 08:53:14 visa Exp $

SUBDIR= ramdisk

.if	make(obj) || make(cleandir) || make(clean)
SUBDIR+= iso
.endif

unconfig:
	cd ramdisk; ${MAKE} unconfig

.include <bsd.subdir.mk>
