#	$OpenBSD: Makefile,v 1.9 2012/08/21 14:46:19 pascal Exp $
S=${.CURDIR}/../../../..
CFLAGS=-I${.CURDIR}/../../include -I${.CURDIR}/../libbug -I${S}/lib/libsa \
	-fomit-frame-pointer
AFLAGS+=-D_LOCORE
NOPIE=

OBJS=bugcrt.o bugcrtS.o

all:	${OBJS}

install:

lint tags:

.include <bsd.prog.mk>
