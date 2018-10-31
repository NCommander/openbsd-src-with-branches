#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.3 2014/10/20 06:55:59 guenther Exp $

PROG = amq
SRCS = amq.c amq_clnt.c amq_xdr.c
MAN  = amq.8
CFLAGS+=-I${.CURDIR}/../include
CFLAGS+=-I${.CURDIR}/../rpcx
.PATH: ${.CURDIR}/../rpcx

.include "../../Makefile.inc"
.include <bsd.prog.mk>
