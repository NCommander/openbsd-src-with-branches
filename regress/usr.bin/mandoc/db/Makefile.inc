# $OpenBSD$

obj-clean:
	rm -rf ${.CURDIR}/obj/*
	rm -f ${.CURDIR}/obj
	cd ${.CURDIR} && ${MAKE} clean
