#	$OpenBSD: Makefile,v 1.2 2001/02/03 08:15:10 niklas Exp $


DIR=	usd/18.msdiffs
SRCS=	ms.diffs
MACROS=	-ms

paper.txt: ${SRCS}
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
