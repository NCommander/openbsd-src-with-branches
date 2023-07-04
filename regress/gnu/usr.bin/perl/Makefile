# $OpenBSD$

.if ! (make(clean) || make(cleandir) || make(obj))
NCPU !=		/sbin/sysctl -n hw.ncpu
.endif

REGRESS_TARGETS +=	build
build:
	${MAKE} -C ${BSDSRCDIR}/gnu/usr.bin/perl -f Makefile.bsd-wrapper obj
	${MAKE} -C ${BSDSRCDIR}/gnu/usr.bin/perl -f Makefile.bsd-wrapper -j ${NCPU} all

REGRESS_TARGETS +=	test
test:
	${MAKE} -C ${BSDSRCDIR}/gnu/usr.bin/perl -f Makefile.bsd-wrapper test

.include <bsd.regress.mk>
