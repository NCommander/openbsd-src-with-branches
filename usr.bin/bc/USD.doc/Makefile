#	$OpenBSD: Makefile,v 1.2 2003/11/03 20:17:38 otto Exp $

DIR=	usd/06.bc
SRCS=	bc
MACROS=	-ms

paper.txt: ${SRCS}
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
