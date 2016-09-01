# $OpenBSD: Makefile,v 1.1 2016/07/30 10:56:13 schwarze Exp $

PROG =		dbm_dump
CPPFLAGS +=	-I${BSDSRCDIR}/usr.bin/mandoc
MOBJS =		dbm.o dbm_map.o
LDADD +=	${MOBJS}
CLEANFILES =	${MOBJS}

regress: dbm_dump

dbm_dump: ${MOBJS}

${MOBJS}:
	@cp ${BSDOBJDIR}/usr.bin/mandoc/$@ . || \
		cp ${BSDSRCDIR}/usr.bin/mandoc/$@ .

.include <bsd.prog.mk>
