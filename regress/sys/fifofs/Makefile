#	$OpenBSD$

PROG=fifotest
FIFO=fi.fo.fum
CLEANFILES+=${FIFO}
CFLAGS+= -Wall

REGRESS_TARGETS=runtest

runtest: ${PROG}
	./${PROG} ${FIFO} | diff - ${.CURDIR}/${PROG}.out

.include <bsd.regress.mk>
