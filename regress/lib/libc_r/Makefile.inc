# $OpenBSD: Makefile.inc,v 1.7 2002/11/16 05:02:48 marc Exp $
# Copyright (c) 1993 Chris Provenzano, proven@athena.mit.edu

LIBPTHREAD=	/usr/lib/libpthread.a

LDADD+=		-pthread
DPADD+=		${LIBPTHREAD}

CFLAGS+=	-Wall # -Werror
#DEBUG=		-ggdb
CFLAGS+=	-DSRCDIR='"${.CURDIR}"'
CFLAGS+=	-I${.CURDIR}/../include

NOMAN=

REGRESS_MAXTIME?=30
