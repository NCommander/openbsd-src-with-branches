#	$OpenBSD: Makefile,v 1.3 1998/01/26 04:16:56 dgregor Exp $

PROG=	ipresend
BINDIR=	/usr/sbin
MAN=	ipresend.1
SRCS=	ipresend.c resend.c \
	ipft_ef.c ipft_hx.c ipft_pc.c ipft_sn.c ipft_td.c ipft_tx.c opt.c \
	sock.c 44arp.c sbpf.c ip.c
CFLAGS+=  -DDOSOCKET -I${.CURDIR}/../common -I${.CURDIR}/../../ipftest \
		-I${.CURDIR}/../../../sbin/ipf \
		-I${.CURDIR}/../../../sys/netinet
.PATH:	${.CURDIR}/../common ${.CURDIR}/../../ipftest \
		${.CURDIR}/../../../sbin/ipf

.include <bsd.prog.mk>
