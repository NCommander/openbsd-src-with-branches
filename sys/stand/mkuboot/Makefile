#	$OpenBSD: Makefile,v 1.2 2008/06/09 15:01:56 deraadt Exp $

MAN=	mkuboot.8

PROG=	mkuboot
DPADD=	${LIBZ}
LDADD=	-lz
CFLAGS=	-DMACHINE_ARCH=\"${MACHINE_ARCH}\"

.include <bsd.prog.mk>
