# $OpenBSD: Makefile,v 1.1 2023/08/12 13:43:22 gnezdo Exp $

REGRESS_TARGETS+=	empty
REGRESS_TARGETS+=	short

CLEANFILES+=	*.in *.out

empty:
	: >$@.in
	ldd $@.in 2>&1 | tee $@.out | grep -q 'incomplete ELF header'

short:
	dd if=/usr/bin/ldd of=$@.in count=128 bs=1 status=none
	ldd $@.in 2>&1 | tee $@.out | grep -q 'incomplete program header'

.include <bsd.regress.mk>
