#	$OpenBSD: Makefile,v 1.2 1996/04/21 23:45:02 deraadt Exp $

PROG=	yacc
SRCS=	closure.c error.c lalr.c lr0.c main.c mkpar.c output.c reader.c \
	skeleton.c symtab.c verbose.c warshall.c
MAN=	yacc.1 yyfix.1

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/yyfix.sh ${DESTDIR}${BINDIR}/yyfix

.include <bsd.prog.mk>
