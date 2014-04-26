#	$OpenBSD: Makefile,v 1.1 2006/12/11 23:10:11 reyk Exp $

PROGDIR=	${.CURDIR}/../../../usr.sbin/traceroute

LDSTATIC=	-static
CFLAGS+=	-I${PROGDIR}
NOMAN=		yes

BINDIR=		/var/www/bin
LINKS=		${BINDIR}/traceroute ${BINDIR}/traceroute6

.include "${PROGDIR}/Makefile"

BINMODE=	000

.PATH:		${PROGDIR}

