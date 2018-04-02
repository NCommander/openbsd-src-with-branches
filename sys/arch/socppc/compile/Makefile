#	$OpenBSD: Makefile,v 1.1 2016/10/14 18:43:01 deraadt Exp $

.if	make(obj) || make(clean) || make(cleandir)
SUBDIR!=find . -type d -maxdepth 1 \! \( -name . -o -name CVS \) | cut -b3-
.endif

.include <bsd.subdir.mk>
