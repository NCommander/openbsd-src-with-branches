#	$OpenBSD: Makefile,v 1.3 1996/06/23 14:30:52 deraadt Exp $

PROG=	authpf
MAN=	authpf.8
SRCS=	authpf.c parse.y pfctl_parser.c
CFLAGS+= -I${.CURDIR}/../../sbin/pfctl -Wall -Werror
.PATH:  ${.CURDIR}/../../sbin/pfctl

.include <bsd.prog.mk>
