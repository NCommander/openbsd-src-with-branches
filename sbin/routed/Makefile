#	$OpenBSD: Makefile,v 1.5 1996/09/06 13:05:51 deraadt Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/19/93

PROG=	routed
SRCS=	if.c input.c main.c output.c parms.c radix.c rdisc.c table.c trace.c
MAN=	routed.8
SUBDIR=	rtquery
# this is for /sys/net/radix.c only
CFLAGS+=-D_ROUTED -D'min(a,b)=(a < b ? a : b)' -D'log=syslog'
CFLAGS+=-D'panic(s)={log(LOG_ERR,s); exit(1);}'

# Do *NOT* turn this flag on unless you like security holes
#CFLAGS+=-DPERMIT_TRACE

.PATH: ${.CURDIR}/../../sys/net

.include <bsd.prog.mk>
