# $OpenBSD: Makefile,v 1.1 2002/06/07 05:02:58 hin Exp $

AFSTOPDIR	= ../..
.include <../../Makefile.inc>

.PATH:	$(ROKENSRC)

PROG	= pts
SRCS	= pts.c \
	  estrdup.c
LDADD	+= ${LIBARLA} -lkrb5 -lcrypto -lasn1 -lreadline -ldes -lcurses
DPADD	+= ${LIBARLA}

.PATH: ${AFSSRCDIR}/appl/pts

SRCS	+= arlalib.c fs_lib.c
CFLAGS	+= -I${AFSSRC}/appl/lib
.PATH: ${AFSSRC}/appl/lib
.PATH: ${AFSSRC}/appl/pts
.PATH: ${AFSSRC}/arlad

.include <bsd.prog.mk>
