#   $OpenBSD: Makefile,v 1.3 1997/07/29 18:03:34 veego Exp $
#   $NetBSD: Makefile,v 1.3 1997/07/29 18:03:34 veego Exp $

.if ${MACHINE} == "amiga"
PROG=		grfconfig
CFLAGS+=	-I${.CURDIR}/../../sys/arch -D${MACHINE}
.endif

MAN=	grfconfig.8

.include <bsd.prog.mk>
