#	$OpenBSD: Makefile,v 1.1 2003/06/26 16:20:04 mickey Exp $

DIR=	usd/16.awk
SRCS=	awk
MACROS=	-ms

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
