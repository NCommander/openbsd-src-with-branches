#	$OpenBSD: Makefile,v 1.2 2001/02/03 08:14:57 niklas Exp $


DIR=	papers/fsinterface
SRCS=	fsinterface.ms
MACROS=	-ms

paper.txt: ${SRCS}
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
