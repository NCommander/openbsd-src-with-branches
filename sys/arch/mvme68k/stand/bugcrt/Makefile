#	$OpenBSD: Makefile,v 1.4 1996/05/10 18:39:12 deraadt Exp $

CFLAGS+=-I${.OBJDIR} -O2 -fomit-frame-pointer

OBJS=bugcrt.o

CLEANFILES+=a.out machine

all:	machine ${OBJS}

bugcrt.o: bugcrt.c
	${CC} ${CFLAGS} -c ${.ALLSRC}
	${LD} -x -r ${.TARGET}
	mv a.out ${.TARGET}

install:

lint tags:

machine:
	rm -f machine; ln -s ${.CURDIR}/../../include machine

.include <bsd.prog.mk>
