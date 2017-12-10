#	$OpenBSD: Makefile,v 1.21 2017/07/10 14:09:59 espie Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-add.c
SRCS+=	addrmatch.c atomicio.c authfd.c authfile.c bitmap.c blocks.c chacha.c \
	cipher-chachapoly.c cipher.c cleanup.c digest-openssl.c ed25519.c \
	fatal.c fe25519.c ge25519.c hash.c krl.c log.c match.c misc.c \
	poly1305.c readpass.c sc25519.c ssh-dss.c ssh-ecdsa.c ssh-ed25519.c \
	ssh-rsa.c sshbuf-getput-basic.c sshbuf-getput-crypto.c sshbuf-misc.c \
	sshbuf.c ssherr.c sshkey.c uidswap.c verify.c xmalloc.c

PROG=	ssh-add
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+= ${LIBCRYPTO} ${LIBUTIL}
