#	$OpenBSD: Makefile,v 1.2 2001/02/03 08:14:59 niklas Exp $


DIR=	papers/nqnfs
SRCS=	nqnfs.me
MACROS=	-me

paper.ps: ${SRCS}
	${PIC} ${SRCS} | ${TBL} | ${ROFF} > ${.TARGET}

paper.txt: ${SRCS}
	${PIC} ${SRCS} | ${TBL} | ${ROFF} -Tascii > ${.TARGET}

.include <bsd.doc.mk>
