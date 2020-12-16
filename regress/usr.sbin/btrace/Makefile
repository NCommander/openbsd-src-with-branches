# $OpenBSD: Makefile,v 1.7 2020/12/06 13:32:18 bluhm Exp $

BTRACE?=		 /usr/sbin/btrace

# scripts that don't need /dev/dt
BT_LANG_SCRIPTS=	arithm beginend boolean comments delete exit map \
			map-unnamed maxoperand min+max+sum multismts nsecs+var

BT_KERN_SCRIPTS=

REGRESS_EXPECTED_FAILURES=	run-maxoperand

.for b in ${BT_LANG_SCRIPTS}
REGRESS_TARGETS+=		run-$b
run-$b:
	${BTRACE} ${.CURDIR}/$b.bt | \
		diff -u ${.CURDIR}/$b.ok /dev/stdin
.endfor

.include <bsd.regress.mk>
