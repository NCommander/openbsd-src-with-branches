#	$OpenBSD: Makefile,v 1.2 2003/12/09 12:42:41 jmc Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/8/93

DIR=	psd/16.lex
SRCS=	lex.ms
MACROS=	-ms

paper.txt: ${SRCS}
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
