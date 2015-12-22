# $OpenBSD: Makefile,v 1.12 2015/12/08 09:03:50 sunil Exp $

all clean:

distribution:
	${INSTALL} -c -o root -g wheel -m 644 aliases \
	    ${DESTDIR}/etc/mail/aliases
	${INSTALL} -c -o root -g wheel -m 644 smtpd.conf \
	    ${DESTDIR}/etc/mail/smtpd.conf
	${INSTALL} -c -o root -g wheel -m 644 spamd.conf \
	    ${DESTDIR}/etc/mail/spamd.conf

.PHONY: distribution
.include <bsd.own.mk>
.include <bsd.sys.mk>
