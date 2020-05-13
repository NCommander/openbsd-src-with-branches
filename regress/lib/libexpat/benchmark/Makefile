# $OpenBSD$

# take any large xml file located in our source tree
XMLFILE?=	${BSDSRCDIR}/gnu/lib/libstdc++/libstdc++/docs/html/17_intro/porting-howto.xml

run-regress-benchmark: ${PROG}
	time ./${PROG} ${XMLFILE} 65535 10000

.include <bsd.regress.mk>
