#	$OpenBSD: Makefile,v 1.2 1998/07/12 05:06:27 todd Exp $

INFO = amdref.info amdref.info-1 amdref.info-2 amdref.info-3 amdref.info-4
SRCS = amdref.texinfo

all: ${INFO}

${INFO}: ${SRCS}
	-makeinfo --no-validate ${.CURDIR}/${SRCS}

clean cleandir:
	-rm -f ${INFO}

install: ${INFO}
	 ${INSTALL} ${INSTALL_COPY} -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} ${INFO} ${DESTDIR}${SHAREDIR}/info
	 install-info --info-dir=${DESTDIR}${SHAREDIR}/info amdref.info

.include <bsd.own.mk>
.include <bsd.obj.mk>
.include <bsd.subdir.mk>
