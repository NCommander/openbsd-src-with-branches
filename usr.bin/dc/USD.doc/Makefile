#	$OpenBSD$

DIR=	usd/05.dc
SRCS=	dc
MACROS=	-ms

paper.ps: ${SRCS}
	${EQN} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
