#	$OpenBSD: Makefile,v 1.2 2001/02/03 08:14:56 niklas Exp $


DIR=	papers/beyond43
SRCS=	beyond43.ms
MACROS=	-ms

paper.txt: ${SRCS}
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
