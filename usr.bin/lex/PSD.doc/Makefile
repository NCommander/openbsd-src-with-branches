#	$OpenBSD$
#	@(#)Makefile	8.1 (Berkeley) 6/8/93

DIR=	psd/16.lex
SRCS=	lex.ms
MACROS=	-ms

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
