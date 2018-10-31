#	$OpenBSD: Makefile,v 1.9 2012/11/02 21:53:16 miod Exp $

# static because it's used by the boot floppy through a chroot.

PROG=	pwd_mkdb
LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.PATH:  ${.CURDIR}/../../../usr.sbin/pwd_mkdb
.include <bsd.prog.mk>
