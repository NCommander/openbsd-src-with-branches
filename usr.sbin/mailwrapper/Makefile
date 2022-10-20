#	$OpenBSD: Makefile,v 1.5 2009/03/16 22:34:13 jacekm Exp $

PROG=	mailwrapper
MAN=	mailwrapper.8 mailer.conf.5

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

afterinstall:
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/sbin/sendmail
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/bin/newaliases
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/bin/mailq
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/sbin/makemap
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/bin/hoststat
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/bin/purgestat
	chown -h ${BINOWN}:${BINGRP} ${DESTDIR}/usr/sbin/sendmail \
	    ${DESTDIR}/usr/bin/newaliases ${DESTDIR}/usr/bin/mailq \
	    ${DESTDIR}/usr/sbin/makemap ${DESTDIR}/usr/bin/hoststat \
	    ${DESTDIR}/usr/bin/purgestat

.include <bsd.prog.mk>
