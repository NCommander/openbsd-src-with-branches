# $OpenBSD$

PROG=	keynote-verify

BINOWN= root
BINGRP= bin
BINMODE= 555
BINDIR= /usr/bin
LDADD=	-lkeynote -lm -lcrypto
MAN=	keynote-verify.1
LEXFLAGS2 = -Pkv -s -i
YACCFLAGS2 = -d -p kv -b z

CLEANFILES+= lex.kv.c z.tab.c z.tab.h

KEYNOTEDIST= lib/libkeynote

KEYNOTE_SRC= ${.CURDIR}/../../${KEYNOTEDIST}

CFLAGS+= -DCRYPTO -I. -I${.CURDIR}

SRCS=	keynote-verify.c z.tab.c lex.kv.c

z.tab.c: keynote-ver.y
	$(YACC.y) $(YACCFLAGS2) ${KEYNOTE_SRC}/keynote-ver.y

lex.kv.c: keynote-ver.l
	$(LEX.l) $(LEXFLAGS2) ${KEYNOTE_SRC}/keynote-ver.l

.PATH:	${KEYNOTE_SRC}

.include<bsd.prog.mk>

