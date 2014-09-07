#	$OpenBSD$

MAN=	mkboot.8
MANSUBDIR=hppa64

.if ${MACHINE} == "hppa64"
PROG=	mkboot
CC=	${HOSTCC}
LDSTATIC=	-static
.else
NOPROG=
.endif

.include <bsd.prog.mk>
