# $OpenBSD: Makefile,v 1.1 2004/11/22 18:49:05 deraadt Exp $

NOPROG=
NOMAN=

# PCI capable systems only
.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "alpha") || (${MACHINE} == "sparc64") || \
    (${MACHINE_ARCH} == "powerpc") || (${MACHINE} == "cats") || \
    (${MACHINE} == "hppa") || (${MACHINE} == "hppa64") || \
    (${MACHINE} == "sgi")

FIRM= 	yds

CLEANFILES+= ${FIRM} build

all:	build
	${.OBJDIR}/build

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${FIRM} ${DESTDIR}/etc/firmware
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/yds-license ${DESTDIR}/etc/firmware
.endif

.include <bsd.prog.mk>

