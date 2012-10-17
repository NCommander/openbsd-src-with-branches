#	$OpenBSD$

PROGDIR=	${.CURDIR}/../../../usr.sbin/bgpctl

LDSTATIC=	-static
CFLAGS+=	-I${PROGDIR} -I${PROGDIR}/../bgpd
NOMAN=		yes

.include "${PROGDIR}/Makefile"

BINDIR=		/var/www/bin
BINMODE=	000

.PATH:		${PROGDIR} ${PROGDIR}/../bgpd
