#	$OpenBSD: Makefile,v 1.13 2017/09/17 16:31:59 deraadt Exp $

SUBDIR= miniroot

.if	make(obj) || make(cleandir) || make(clean)
SUBDIR+= iso
.endif

unconfig:
	cd miniroot; ${MAKE} unconfig

.include <bsd.subdir.mk>
