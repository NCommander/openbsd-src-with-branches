# $OpenBSD$

NOPROG=
NOMAN=
CC=${HOSTCC}

# sparc64 only
.if (${MACHINE} == "sparc64")

FIRM=	afb

PROG=	build

CLEANFILES+= ${FIRM} ${PROG}

all: ${FIRM}

${FIRM}: build
	${.OBJDIR}/build

realinstall:

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${FIRM} ${DESTDIR}/etc/firmware
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/afb-license ${DESTDIR}/etc/firmware
.endif

.include <bsd.prog.mk>
