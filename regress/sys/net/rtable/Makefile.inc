#	$OpenBSD: Makefile.inc,v 1.5 2019/06/24 12:36:15 visa Exp $

TOPDIR=		${.CURDIR}/..

.ifdef RADIX
SRCS+=		radix.c
.else
SRCS+=		art.c
CFLAGS+=	-DART
.endif

CPPFLAGS+=	-I${TOPDIR} -Wall \
		-D'satosin(sa)=((struct sockaddr_in *)(sa))' \
		-D'satosin6(sa)=((struct sockaddr_in6 *)(sa))'

.PATH:		${TOPDIR} ${TOPDIR}/../../../../sys/net
