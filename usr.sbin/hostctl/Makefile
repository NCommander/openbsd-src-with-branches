#	$OpenBSD$

.if ${MACHINE} == "i386" || ${MACHINE} == "amd64"

PROG=		hostctl

CFLAGS+=	-Wall

.else
NOPROG=		yes
.endif

MAN=		hostctl.8
MANSUBDIR=	i386 amd64

.include <bsd.prog.mk>
