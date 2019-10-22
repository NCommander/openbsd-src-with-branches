#	$OpenBSD: Makefile,v 1.2 2006/02/04 13:59:39 kettenis Exp $

PROG=	blt
LDADD+=	-L/usr/X11R6/lib -L/usr/X11R6/lib/modules -lfb
LDADD+=	-Wl,-rpath /usr/X11R6/lib/modules
LDADD+=	-Wl,--allow-shlib-undefined

.include <bsd.regress.mk>
