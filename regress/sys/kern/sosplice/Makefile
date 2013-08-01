#	$OpenBSD: Makefile,v 1.2 2013/01/08 21:42:19 bluhm Exp $

SUBDIR +=	perf

.if defined(REGRESS_FULL)
SUBDIR +=	error
SUBDIR +=	tcp
SUBDIR +=	udp
.endif

.include <bsd.subdir.mk>
