#	$OpenBSD: Makefile,v 1.3 2002/06/06 22:24:27 mickey Exp $

PROG=	authpf
MAN=	authpf.8
BINOWN= root
BINGRP= authpf
BINMODE= 6555
SRCS=	authpf.c parse.y pfctl_parser.c
CFLAGS+= -I${.CURDIR}/../../sbin/pfctl -Wall -Werror
.PATH:  ${.CURDIR}/../../sbin/pfctl

.include <bsd.prog.mk>
