# $OpenBSD$

SUBDIR	= usr.bin/asn1_compile lib usr.bin libexec usr.sbin

build:
	( cd lib/roken && ${MAKE} )
	( cd usr.bin/asn1_compile && ${MAKE} && ${MAKE} install )
	${MAKE} includes
	${MAKE} depend
	( cd lib && ${MAKE} && ${MAKE} install )
	${MAKE}
	${MAKE} install

.include <bsd.subdir.mk>
