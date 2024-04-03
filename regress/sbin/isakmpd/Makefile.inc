#	$OpenBSD: Makefile.inc,v 1.2 2017/07/04 18:02:21 bluhm Exp $

TOPSRC=		${.CURDIR}/../../../../sbin/isakmpd
TOPOBJ!!=	${MAKE} -C ${TOPSRC} -V .OBJDIR
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/openbsd -I${TOPOBJ} -Wall
DEBUG=		-g
CLEANFILES+=	stamp-generated

${PROG}: stamp-generated

stamp-generated:
	${MAKE} -C ${TOPSRC} generated
	date >$@

.PATH:		${.CURDIR}/../common
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/openbsd
