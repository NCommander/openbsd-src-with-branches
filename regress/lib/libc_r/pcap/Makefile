# $OpenBSD: Makefile,v 1.7 2002/05/11 00:20:20 espie Exp $

PROG=	pcap

DPADD+= ${LIBPCAP}
LDADD+= -lpcap

REGRESS_TARGETS=		sudo-regress-${PROG}
REGRESS_ROOT_TARGETS=	${REGRESS_TARGETS}

sudo-regress-${PROG}: ${PROG}
	${SUDO} ./${PROG}

.include <bsd.regress.mk>
