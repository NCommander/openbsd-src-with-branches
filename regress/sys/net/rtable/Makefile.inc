#	$OpenBSD: Makefile.inc,v 1.7 2020/11/07 09:53:21 denis Exp $

TOPDIR=		${.CURDIR}/..

SRCS+=		art.c

CPPFLAGS+=	-I${TOPDIR} -Wall

.PATH:		${TOPDIR} ${TOPDIR}/../../../../sys/net
