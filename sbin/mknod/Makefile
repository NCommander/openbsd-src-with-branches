#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:36:55 deraadt Exp $

PROG=	mknod
MAN=	mknod.8 mkfifo.1
LINKS=	${BINDIR}/mknod ${BINDIR}/mkfifo

.include <bsd.prog.mk>

beforeinstall:
	ln -f -s ../../sbin/mknod ${DESTDIR}/usr/bin/mkfifo
