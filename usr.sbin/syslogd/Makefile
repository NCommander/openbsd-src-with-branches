#	$OpenBSD: Makefile,v 1.6 2014/10/05 18:14:01 bluhm Exp $

PROG=	syslogd
SRCS=	syslogd.c ttymsg.c privsep.c privsep_fdpass.c ringbuf.c evbuffer_tls.c
MAN=	syslogd.8 syslog.conf.5
LDADD=	-levent -ltls -lssl -lcrypto
DPADD=	${LIBEVENT} ${LIBTLS} ${LIBSSL} ${LIBCRYPTO}

.include <bsd.prog.mk>
