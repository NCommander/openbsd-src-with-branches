#	$OpenBSD$
PROG=	generic
SRCS+=	main.c manager.c parse_tame.c actions.c
NOMAN=	yes

CFLAGS+=	-Wall

REGRESS_TARGETS+=	test_normal test_systrace
REGRESS_SKIP_TARGETS+=	test_systrace

test_normal: ${PROG}
	env LD_BIND_NOW=1 ./${PROG} | diff -I OpenBSD -u ${.CURDIR}/tests.out -

test_systrace: ${PROG}
	env LD_BIND_NOW=1 systrace -A ./${PROG} | diff -I OpenBSD -u ${.CURDIR}/tests.out -

regenerate: ${PROG}
	echo '#	$$OpenBSD$$' > ${.CURDIR}/tests.out
	env LD_BIND_NOW=1 ./${PROG} | tee -a ${.CURDIR}/tests.out

.include <bsd.regress.mk>
