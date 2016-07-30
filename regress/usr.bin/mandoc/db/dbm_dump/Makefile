# $OpenBSD$

PROG =		dbm_dump
CPPFLAGS +=	-I${BSDSRCDIR}/usr.bin/mandoc
LDADD +=	${BSDOBJDIR}/usr.bin/mandoc/dbm.o
LDADD +=	${BSDOBJDIR}/usr.bin/mandoc/dbm_map.o

.include <bsd.prog.mk>
