#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:34:44 deraadt Exp $

PROG=	chmod
CFLAGS+=-DSUPPORT_DOT
MAN=	chmod.1 chgrp.1 chown.8
LINKS=	${BINDIR}/chmod ${BINDIR}/chgrp \
	${BINDIR}/chmod /sbin/chown

# XXX compatibility
afterinstall:
	ln -fs ${DESTDIR}/sbin/chown ${DESTDIR}/usr/sbin/chown
	ln -fs ${BINDIR}/chgrp ${DESTDIR}/usr/sbin/chgrp

.include <bsd.prog.mk>
