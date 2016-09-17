#	$OpenBSD: Makefile,v 1.2 2014/12/23 19:32:16 pascal Exp $

PROGDIR=	${.CURDIR}/../../../sbin/ping

LDSTATIC=	${STATIC}
CFLAGS+=	-I${PROGDIR}
NOMAN=		yes

BINDIR=		/var/www/bin
LINKS=		${BINDIR}/ping ${BINDIR}/ping6

.include "${PROGDIR}/Makefile"

BINMODE=	000

.PATH:		${PROGDIR}
