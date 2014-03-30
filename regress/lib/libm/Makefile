#	$OpenBSD: Makefile,v 1.12 2012/10/25 20:15:58 miod Exp $

SUBDIR+= exp fenv floor fpaccuracy lgamma nextafter rint round \
	 tgamma toint trivial1

.if defined(REGRESS_FULL)
SUBDIR+= cephes
.endif

install:

.include <bsd.subdir.mk>
