#	$OpenBSD: Makefile.inc,v 1.17 2003/06/01 17:00:30 deraadt Exp $

all:	${.CURDIR}/bcopy.S ${.CURDIR}/spcopy.S

${.CURDIR}/bcopy.S: bcopy.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@m4 -DNAME=bcopy ${.ALLSRC} > ${.TARGET}

${.CURDIR}/spcopy.S: bcopy.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@m4 -DNAME=spcopy ${.ALLSRC} > ${.TARGET}

.include <bsd.own.mk>
.include <bsd.obj.mk>
