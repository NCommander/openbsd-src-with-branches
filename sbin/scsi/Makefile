#	$OpenBSD: Makefile,v 1.1 1996/06/12 11:21:41 deraadt Exp $

.if (${MACHINE_ARCH} == "vax")
CFLAGS+=-O1
.endif

PROG=	scsi
MAN=	scsi.8
LDADD=	-lutil
DPADD=	${LIBUTIL}

.include <bsd.prog.mk>
