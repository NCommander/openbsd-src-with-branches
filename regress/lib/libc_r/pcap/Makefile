# $OpenBSD: Makefile,v 1.5 2002/01/03 00:43:47 art Exp $

PROG=	pcap

LDADD+= -lpcap

REGRESSTARGETS=		sudo-regress-${PROG}
REGRESSROOTTARGETS=	${REGRESSTARGETS}

sudo-regress-${PROG}: ${PROG}
	${SUDO} ./${PROG}

.include <bsd.regress.mk>
