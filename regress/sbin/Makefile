#	$OpenBSD: Makefile,v 1.6 2006/05/26 04:02:59 deraadt Exp $

SUBDIR+= route

.if defined(REGRESS_FULL)
SUBDIR+= ipsecctl pfctl
.endif

install:

.include <bsd.subdir.mk>
