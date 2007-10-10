#	$OpenBSD: Makefile,v 1.6 1996/05/16 02:21:58 chuck Exp $
#
# DO NOT OPTMIZE bugcrt (i.e. no "-O2")
#
S=${.CURDIR}/../../../..
CFLAGS=-I${.CURDIR}/../../include -I${.CURDIR}/../libbug -I${S}/lib/libsa \
	-fomit-frame-pointer

OBJS=bugcrt.o

CLEANFILES+=a.out

all:	${OBJS}

bugcrt.o: bugcrt.c
	${COMPILE.c} -c $<
	${LD} -x -r ${.TARGET}
	mv a.out ${.TARGET}

install:

lint tags:

.include <bsd.prog.mk>
