#	$OpenBSD: Makefile,v 1.3 1999/01/13 07:26:04 niklas Exp $

.if ${MACHINE} == "i386"
PROG=		fontedit
CLEANFILES+=	core.fontedit fontedit.core
.else
NOPROG=		yes
.endif
MAN=	fontedit.1
MANSUBDIR=i386

.include <bsd.prog.mk>
