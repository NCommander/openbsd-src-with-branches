#	$OpenBSD: Makefile,v 1.26 2012/07/28 19:07:07 guenther Exp $
#	$NetBSD: Makefile,v 1.1 1997/12/30 23:27:11 cgd Exp $

SUBDIR+= basename bc dc diff diff3 dirname file grep gzip gzsig
SUBDIR+= m4 mandoc sdiff sed sort tsort
SUBDIR+= xargs

.if defined(REGRESS_FULL)
SUBDIR+= cap_mkdb libtool make patch pkg-config rcs ssh
.endif

.include <bsd.subdir.mk>
