# $OpenBSD: Makefile,v 1.29 2000/03/10 22:04:18 niklas Exp $

SUBDIR	= lib usr.bin usr.sbin libexec

build:
	${MAKE} includes
	${MAKE} depend
	( cd ${.CURDIR}/lib && ${MAKE} && ${MAKE} install )
	${MAKE}
	${MAKE} install

.include <bsd.subdir.mk>
