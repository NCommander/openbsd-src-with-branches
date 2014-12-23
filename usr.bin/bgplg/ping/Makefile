#	$OpenBSD: Makefile,v 1.1 2006/12/11 23:10:11 reyk Exp $

PROGDIR=	${.CURDIR}/../../../sbin/ping

LDSTATIC=	${STATIC}
CFLAGS+=	-I${PROGDIR}
NOMAN=		yes

.include "${PROGDIR}/Makefile"

BINDIR=		/var/www/bin
BINMODE=	000

.PATH:		${PROGDIR}
