#	$OpenBSD: Makefile,v 1.1 2005/08/31 20:53:49 kettenis Exp $

PROG=	blt
LDADD+=	-L/usr/X11R6/lib/modules -R/usr/X11R6/lib/modules -lfb
LDADD+=	-Wl,--allow-shlib-undefined

.include <bsd.regress.mk>
