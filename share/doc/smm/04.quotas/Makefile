#	$OpenBSD: Makefile,v 1.2 2001/02/03 08:15:07 niklas Exp $


DIR=	smm/04.quotas
SRCS=	quotas.ms
MACROS=	-ms

paper.txt: ${SRCS}
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
