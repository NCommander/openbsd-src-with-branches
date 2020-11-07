#	$OpenBSD: Makefile.inc,v 1.6 2020/11/06 15:26:38 bluhm Exp $

TOPDIR=		${.CURDIR}/..

.ifdef RADIX
SRCS+=		radix.c
.else
SRCS+=		art.c
CFLAGS+=	-DART
.endif

CPPFLAGS+=	-I${TOPDIR} -Wall

.PATH:		${TOPDIR} ${TOPDIR}/../../../../sys/net
