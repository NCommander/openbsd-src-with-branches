#	$OpenBSD: Makefile.inc,v 1.1 2015/11/04 09:45:52 mpi Exp $

TOPDIR=		${.CURDIR}/..

.ifdef RADIX
SRCS+=		radix.c
.else
SRCS+=		art.c
CFLAGS+=	-DART
.endif

CPPFLAGS+=	-I${TOPDIR} -Wall -DSMALL_KERNEL

.PATH:		${TOPDIR} ${TOPDIR}/../../../../sys/net
