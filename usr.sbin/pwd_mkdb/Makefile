#	$OpenBSD: Makefile,v 1.7 1999/06/06 13:03:05 espie Exp $

# static because it's used by the boot floppy through a chroot.

LDSTATIC=	${STATIC}
PROG=	pwd_mkdb
MAN=	pwd_mkdb.8
LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.if (${MACHINE_ARCH} == "m68k")
CFLAGS+=-O1
.endif

.include <bsd.prog.mk>
