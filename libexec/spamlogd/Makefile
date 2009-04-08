#	$OpenBSD: Makefile,v 1.5 2006/11/26 11:31:14 deraadt Exp $

PROG=	spamlogd
SRCS=	spamlogd.c sync.c
MAN=	spamlogd.8

CFLAGS+= -Wall -Wstrict-prototypes -I${.CURDIR}/../spamd
LDADD+= -lpcap -lcrypto
DPADD+=	${LIBPCAP}
.PATH:  ${.CURDIR}/../spamd

.include <bsd.prog.mk>
