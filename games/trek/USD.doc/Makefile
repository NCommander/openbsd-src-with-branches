#	$OpenBSD: Makefile,v 1.2 1995/04/22 10:59:48 cgd Exp $
#	$NetBSD: Makefile,v 1.2 1995/04/22 10:59:48 cgd Exp $

DIR=	usd/31.trek
SRCS=	trek.me
MACROS=	-me

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
