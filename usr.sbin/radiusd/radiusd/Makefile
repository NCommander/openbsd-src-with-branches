#	$OpenBSD$
PROG=		radiusd
BINDIR=		/usr/sbin
MAN=		radiusd.8 radiusd.conf.5
SRCS=		radiusd.c parse.y log.c util.c imsg_subr.c
LDADD+=		-lradius -lcrypto -levent -lutil
DPADD=		${LIBRADIUS} ${LIBCRYPTO} ${LIBEVENT} ${LIBUTIL}

.include <bsd.prog.mk>
