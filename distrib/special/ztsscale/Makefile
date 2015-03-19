#	$OpenBSD: Makefile,v 1.4 2011/07/08 22:34:05 martynas Exp $

.PATH: ${.CURDIR}/../../../usr.sbin/ztsscale

.if ${MACHINE} == "zaurus"
SRCS=	ztsscale.c

PROG=	ztsscale
.else
NOPROG=yes
.endif

.include <bsd.prog.mk>
