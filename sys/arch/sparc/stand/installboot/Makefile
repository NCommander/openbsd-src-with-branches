#	$OpenBSD$


MAN=	installboot.8
MANSUBDIR=sparc
LDSTATIC=-static

.if ${MACHINE} == "sparc"
BINDIR=	/usr/sbin
PROG=	installboot
.else
NOPROG=
.endif

.include <bsd.prog.mk>
