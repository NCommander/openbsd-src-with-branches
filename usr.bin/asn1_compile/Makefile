# $OpenBSD: Makefile,v 1.4 2002/05/30 20:06:23 hin Exp $

PROG=	asn1_compile
NOMAN=
SRCS=	parse.c lex.l main.c hash.c symbol.c gen.c \
        gen_encode.c gen_decode.c gen_free.c gen_length.c gen_copy.c \
        gen_glue.c getarg.c

CFLAGS+=-I. -DFALSE=0 -DTRUE=1

CLEANFILES=	parse.[ch] lex.c

parse.h parse.c:	parse.y
	${YACC} -d -o parse.c ${.ALLSRC}

.include <bsd.prog.mk>
