# $OpenBSD: Makefile,v 1.31 2000/09/04 18:02:42 hin Exp $

SUBDIR	= lib usr.bin usr.sbin libexec doc

build:
	${MAKE} includes
	${MAKE} depend
	cd ${.CURDIR}/lib && ${MAKE} && ${SUDO} ${MAKE} install
	${MAKE}
	${SUDO} ${MAKE} install

.include <bsd.subdir.mk>
