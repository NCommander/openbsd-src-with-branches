#	$OpenBSD: Makefile,v 1.1 2012/10/12 14:00:02 jsing Exp $

MAN=
MLINKS=

.if ${MACHINE} == "amd64"
PROG=	fdboot
COPTS=	-DFDBOOT
.endif

.PATH:	${.CURDIR}/../boot

.include "${.CURDIR}/../boot/Makefile"
