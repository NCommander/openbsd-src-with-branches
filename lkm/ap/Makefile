#	$OpenBSD: Makefile,v 1.1 1996/03/05 11:25:24 mickey Exp $

LKM=	ap
SRCS+=	aperture.c lkm.c version.c
NOMAN=	noman

.ifmake !(install)
SUBDIR=	aptest
.endif

afterinstall:
	install -c -o ${LKMOWN} -g ${LKMGRP} -m 555 ${.CURDIR}/apinstall \
		${DESTDIR}${LKMDIR}

.include <bsd.lkm.mk>
