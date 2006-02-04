#	$OpenBSD: Makefile,v 1.1 2005/08/15 18:53:42 kettenis Exp $

PROG=	bltone
LDADD+=	-L/usr/X11R6/lib/modules -R/usr/X11R6/lib/modules -lfb
LDADD+=	-Wl,--allow-shlib-undefined

.include <bsd.regress.mk>
