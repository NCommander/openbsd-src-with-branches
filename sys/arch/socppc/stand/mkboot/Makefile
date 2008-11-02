#	$OpenBSD: Makefile,v 1.1 2008/05/15 21:32:20 kettenis Exp $

.include "${.CURDIR}/../Makefile.inc"

MAN=	mkboot.8

.if ${MACHINE} == "socppc"
PROG=	mkboot
DPADD=	${LIBZ}
LDADD=	-lz
.else
NOPROG=
.endif

.include <bsd.prog.mk>
