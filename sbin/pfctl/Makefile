#
#	$OpenBSD: Makefile,v 1.3 2001/06/26 22:38:28 smart Exp $

CLEANFILES+=parse.c y.tab.h

PROG=	pfctl
SRCS=	pfctl.c parse.c pfctl_parser.c
MAN=	pfctl.8

DEBUG=-g

CFLAGS+= -Wall

.include <bsd.prog.mk>
