#       $OpenBSD: Makefile,v 1.1 1996/10/14 05:14:57 millert Exp $

PROG=	visudo
MAN=	visudo.8
CFLAGS+=-I${.CURDIR}/../sudo -I.
SRCS=	sudo.tab.c lex.yy.c visudo.c
CLEANFILES+=sudo.tab.c sudo.tab.h lex.yy.c

LDADD=  -lcompat
DPADD=  ${LIBCOMPAT}

BINOWN=	root
BINMODE=111
BINDIR?=/usr/sbin

.include <bsd.prog.mk>

lex.yy.c: ${.CURDIR}/../sudo/parse.lex
	rm -f lex.yy.c
	$(LEX) ${.CURDIR}/../sudo/parse.lex

sudo.tab.c sudo.tab.h: ${.CURDIR}/../sudo/parse.yacc
	$(YACC) -b sudo -d ${.CURDIR}/../sudo/parse.yacc
