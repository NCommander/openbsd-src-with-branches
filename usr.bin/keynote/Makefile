#	$OpenBSD$

all:
	@cd ${.CURDIR}/../../lib && ${MAKE} libkeynote

install:
	@cd ${.CURDIR}/../../lib/libkeynote && ${MAKE} -f Makefile.bsd-wrapper install-keynote

clean:
	@cd ${.CURDIR}/../../lib/libkeynote && ${MAKE} -f Makefile.bsd-wrapper clean

cleandir:
	@cd ${.CURDIR}/../../lib/libkeynote && ${MAKE} -f Makefile.bsd-wrapper cleandir

.include <bsd.prog.mk>
