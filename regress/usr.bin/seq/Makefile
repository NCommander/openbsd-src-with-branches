#	$OpenBSD: Makefile,v 1.1 2023/06/12 20:19:45 millert Exp $

SEQ?=	/usr/bin/seq

CLEANFILES+=	*.out

REGRESS_TARGETS+=seqtest
seqtest:
	sh ${.CURDIR}/$@.sh ${SEQ} $@.out
	diff ${.CURDIR}/$@.expected $@.out

.include <bsd.regress.mk>
