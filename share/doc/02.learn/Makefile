#	$OpenBSD$
#	@(#) This version did not come via Berkeley, but direct from Bell Labs.

DIR=	usd/02.learn
SRCS=	COPYRIGHT learn.ms
MACROS=	-ms

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
