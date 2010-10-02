# $OpenBSD$
.include <bsd.own.mk>
X11BASE?=	/usr/X11R6
X11ETC?=	/etc/X11
.if exists(${X11BASE}/share/mk/bsd.xconf.mk)
. include	"${X11BASE}/share/mk/bsd.xconf.mk"
.endif
