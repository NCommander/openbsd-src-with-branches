#	$OpenBSD: Makefile,v 1.9 1996/12/08 15:00:08 downsj Exp $

MAN=	ypinit.8

DISTFILES= Makefile.main Makefile.yp
SHFILES= ypinit.sh

install:
.for script in ${SHFILES}
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${script} ${DESTDIR}${BINDIR}/${script:R}
.endfor
.for script in ${DISTFILES}
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${script} ${DESTDIR}/var/yp/${script}.dist
.endfor
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/README ${DESTDIR}/var/yp/README

distribution:
.for script in ${DISTFILES}
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${script} ${DESTDIR}/var/yp/${script}
.endfor

.include <bsd.prog.mk>
