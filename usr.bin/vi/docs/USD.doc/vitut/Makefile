#	$OpenBSD: Makefile,v 1.3 2001/01/29 01:58:36 niklas Exp $


DIR=	usd/12.vi
SRCS=	vi.in vi.chars
EXTRA=	vi.apwh.ms vi.summary
MACROS=	-ms
CLEANFILES+=summary.* viapwh.* vitut.txt

paper.ps: ${SRCS} summary.ps viapwh.ps
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}
vitut.txt: ${SRCS} summary.txt viapwh.txt
	${TBL} ${SRCS} | groff ${MACROS} -Tascii > ${.TARGET}

summary.ps: vi.summary
	${TBL} vi.summary | ${ROFF} > ${.TARGET}
summary.txt: vi.summary
	${TBL} vi.summary | groff ${MACROS} -Tascii > ${.TARGET}

viapwh.ps: vi.apwh.ms
	${ROFF} vi.apwh.ms > ${.TARGET}
viapwh.txt: vi.apwh.ms
	groff ${MACROS} -Tascii vi.apwh.ms > ${.TARGET}

.include <bsd.doc.mk>
