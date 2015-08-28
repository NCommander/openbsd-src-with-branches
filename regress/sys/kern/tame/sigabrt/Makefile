#	$OpenBSD$
PROG=	sigabrt
NOMAN=	yes

run-regress-${PROG}: ${PROG}
	rm -f ./${PROG}.core
	if ./${PROG}; then false; else true; fi
	if [ ! -e ./${PROG}.core ]; then echo "No coredump"; false; fi
	./${PROG} | diff -I OpenBSD -u ${.CURDIR}/${PROG}.out -

.include <bsd.regress.mk>
