#	$OpenBSD: Makefile,v 1.9 2019/10/16 17:43:29 claudio Exp $

PROG=	rpki-client
SRCS=	as.c cert.c cms.c crl.c io.c ip.c log.c main.c mft.c \
	output-bgpd.c output-bird.c output-csv.c output-json.c \
	roa.c rsync.c tal.c validate.c x509.c
MAN=	rpki-client.8

LDADD+= -lcrypto
DPADD+= ${LIBCRYPTO}

.include <bsd.prog.mk>
