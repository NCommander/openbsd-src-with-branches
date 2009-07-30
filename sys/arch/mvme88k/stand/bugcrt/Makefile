#	$OpenBSD: Makefile,v 1.11 2007/10/30 06:03:16 deraadt Exp $

S=${.CURDIR}/../../../..

CFLAGS+=-I${.CURDIR}/../../include -I${.CURDIR}/../libbug -I${S}/lib/libsa
STAGE1_CFLAGS=-DSTAGE1
STAGE2_CFLAGS=-DSTAGE2
NETBOOT_CFLAGS=-DNETBOOT
SRCS=crt0.S
OBJS=stage1crt.o stage2crt.o netcrt.o

CLEANFILES+=stage1crt.out stage2crt.out netcrt.out ${OBJS}

all:	${OBJS}

stage1crt.o: crt0.S
	${CC} ${CFLAGS} ${STAGE1_CFLAGS} -c ${.CURDIR}/crt0.S -o ${.TARGET}
	${LD} -x -r ${.TARGET} -o ${.TARGET}ut
	mv ${.TARGET}ut ${.TARGET}

stage2crt.o: crt0.S
	${CC} ${CFLAGS} ${STAGE2_CFLAGS} -c ${.CURDIR}/crt0.S -o ${.TARGET}
	${LD} -x -r ${.TARGET} -o ${.TARGET}ut
	mv ${.TARGET}ut ${.TARGET}

netcrt.o: crt0.S
	${CC} ${CFLAGS} ${NETBOOT_CFLAGS} -c ${.CURDIR}/crt0.S -o ${.TARGET}
	${LD} -x -r ${.TARGET} -o ${.TARGET}ut
	mv ${.TARGET}ut ${.TARGET}

install:

lint tags:

.include <bsd.prog.mk>
