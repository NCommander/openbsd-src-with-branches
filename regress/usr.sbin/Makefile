#	$OpenBSD: Makefile,v 1.3 2013/02/05 22:40:57 bluhm Exp $

SUBDIR+= mtree

.if defined(REGRESS_FULL)
SUBDIR += pkg_add relayd
.endif

.include <bsd.subdir.mk>
