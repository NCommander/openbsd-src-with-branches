#	$OpenBSD: Makefile,v 1.2 2001/01/28 19:34:31 niklas Exp $

PROG =	rpc.rusersd
SRCS =	rusersd.c rusers_proc.c
MAN =	rpc.rusersd.8
MLINKS=	rpc.rusersd.8 rusersd.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>

