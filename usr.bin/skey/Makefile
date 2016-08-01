#	$OpenBSD: Makefile,v 1.14 2014/03/20 20:39:13 naddy Exp $

PROG=	skey
MAN=	skey.1 skeyprune.8
LINKS=	${BINDIR}/skey ${BINDIR}/otp-md5 \
	${BINDIR}/skey ${BINDIR}/otp-sha1 \
	${BINDIR}/skey ${BINDIR}/otp-rmd160
DPADD=	${LIBSKEY}
LDADD=	-lskey

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -m 755 ${.CURDIR}/skeyprune.pl \
		${DESTDIR}${BINDIR}/skeyprune

.include <bsd.prog.mk>
