#	$OpenBSD: Makefile,v 1.6 2002/07/13 02:34:58 art Exp $

PROG=	authpf
MAN=	authpf.8
BINOWN= root
BINGRP= authpf
BINMODE= 6555
SRCS=	authpf.c parse.y pfctl_parser.c pf_print_state.c pfctl_altq.c
CFLAGS+= -I${.CURDIR}/../../sbin/pfctl -Wall -Werror
.PATH:  ${.CURDIR}/../../sbin/pfctl

LDADD+= -lm
DPADD+= ${LIBM}

CLEANFILES+=y.tab.h

.include <bsd.prog.mk>
