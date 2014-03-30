#	$OpenBSD$

PROGDIR=	${.CURDIR}/../../../sbin/ping6

LDSTATIC=	-static
CFLAGS+=	-I${PROGDIR}
NOMAN=		yes

.include "${PROGDIR}/Makefile"

BINDIR=		/var/www/bin
BINMODE=	000

.PATH:		${PROGDIR}
