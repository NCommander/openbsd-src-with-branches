#
#	$OpenBSD: Makefile,v 1.4 2003/03/02 12:19:57 dhartmei Exp $
#
FILES=	queue1 queue2 queue3 ackpri faq-example1 faq-example2 faq-example3
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	install -c -m 0444 ${FILES} ${DESTDIR}${BINDIR}/pf/

.include <bsd.prog.mk>
