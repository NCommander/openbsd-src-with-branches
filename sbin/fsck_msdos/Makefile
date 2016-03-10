#	$OpenBSD: Makefile,v 1.3 1997/03/02 05:25:50 millert Exp $

PROG=	fsck_msdos
MAN=	fsck_msdos.8
SRCS=	main.c check.c boot.c fat.c dir.c fsutil.c
.PATH: ${.CURDIR}/../fsck
CFLAGS+= -I${.CURDIR}/../fsck
DPADD+=	${LIBUTIL}
LDADD+=	-lutil

.include <bsd.prog.mk>
