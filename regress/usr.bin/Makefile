#	$OpenBSD: Makefile,v 1.38 2016/09/25 15:42:40 otto Exp $

SUBDIR+= basename bc calendar colrm column dc diff diff3 dirname doas
SUBDIR+= file fmt fold grep gzip
SUBDIR+= jot m4 mandoc openssl rev sdiff sed signify sort tsort ul
SUBDIR+= xargs

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= cap_mkdb libtool make patch pkg-config rcs ssh
.endif

.include <bsd.subdir.mk>
