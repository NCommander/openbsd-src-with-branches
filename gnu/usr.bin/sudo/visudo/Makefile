#       $OpenBSD: Makefile,v 1.6 1996/07/31 17:44:23 deraadt Exp $

PROG=	visudo
MAN=	visudo.8
CFLAGS+=-I${.CURDIR}/../sudo -I.
SRCS=	y.tab.c lex.yy.c visudo.c
CLEANFILES+=y.tab.c y.tab.h lex.yy.c

LDADD=  -lcompat
DPADD=  ${LIBCOMPAT}

BINOWN=	root
BINMODE=111
BINDIR?=/usr/sbin

.include <bsd.prog.mk>

lex.yy.c: ${.CURDIR}/../sudo/parse.lex
	rm -f lex.yy.c
	$(LEX) ${.CURDIR}/../sudo/parse.lex

y.tab.c y.tab.h: ${.CURDIR}/../sudo/parse.yacc
	$(YACC) -d ${.CURDIR}/../sudo/parse.yacc
