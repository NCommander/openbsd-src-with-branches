# $OpenBSD: Makefile,v 1.32 2001/06/05 10:10:55 hin Exp $

SUBDIR	= lib usr.bin usr.sbin libexec doc

build:
	${SUDO} ${MAKE} includes
	${MAKE} depend
	cd ${.CURDIR}/lib && ${MAKE} && ${SUDO} ${MAKE} install
	${MAKE}
	${SUDO} ${MAKE} install

.include <bsd.subdir.mk>
