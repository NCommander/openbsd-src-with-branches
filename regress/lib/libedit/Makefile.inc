#	$OpenBSD: Makefile.inc,v 1.3 2017/07/05 15:31:45 bluhm Exp $

TOPSRC=		${.CURDIR}/../../../../lib/libedit
TOPOBJ!!=	${MAKE} -C ${TOPSRC} -V .OBJDIR
CFLAGS+=	-I${TOPSRC} -I${TOPOBJ}

${PROG} ${PROGS}: ${TOPOBJ}/common.h

${TOPOBJ}/common.h:
	${MAKE} -C ${TOPSRC} generated
