#	$OpenBSD: Makefile,v 1.1 2002/06/11 22:10:22 provos Exp $

NAMED_TESTS = random-inserts sequential-inserts \
	sequential-removes random-removes remove-nfind \
	remove-pfind node-iterations iteration-macros insert-next \
	insert-prev benchmarks

REGRESS_TARGETS = all-tests ${NAMED_TESTS}

PROGS = rb-test rbt-test

all-tests: rb-test rbt-test
	@echo "===== testing RB macros ====="
	./rb-test
	@echo "===== testing RBT macros ====="
	./rbt-test

${NAMED_TESTS}: rb-test rbt-test
	@echo "===== testing RB macros ====="
	./rb-test $@
	@echo "===== testing RBT macros ====="
	./rbt-test $@

.include <bsd.regress.mk>
