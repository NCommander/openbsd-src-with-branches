#	$OpenBSD: Makefile,v 1.6 1997/09/18 17:43:47 deraadt Exp $

.if	!make(install)
SUBDIR= special
.endif
.if exists(${MACHINE})
SUBDIR+= ${MACHINE}
.endif
SUBDIR+= notes

.include <bsd.subdir.mk>
