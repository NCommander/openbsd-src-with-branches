#	$OpenBSD: Makefile,v 1.2 2001/02/03 08:15:11 niklas Exp $


DIR=	usd/20.meref
SRCS=	ref.me
MACROS=	-me

paper.txt: ${SRCS}
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
