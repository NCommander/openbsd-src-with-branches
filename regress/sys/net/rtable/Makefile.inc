#	$OpenBSD$

TOPDIR=		${.CURDIR}/..

.ifdef ART
SRCS+=		art.c
CFLAGS+=	-DART
.else
SRCS+=		radix.c
.endif

CPPFLAGS+=	-I${TOPDIR} -Wall -DSMALL_KERNEL

.PATH:		${TOPDIR} ${TOPDIR}/../../../../sys/net
