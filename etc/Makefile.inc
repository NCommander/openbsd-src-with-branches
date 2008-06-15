#	$OpenBSD: Makefile.inc,v 1.1 2008/06/14 23:09:05 todd Exp $

MAKEDEVSRC=	${.CURDIR}/../MAKEDEV.mi
MAKEDEVSRC+=	${.CURDIR}/../MAKEDEV.sub
MAKEDEVSRC+=	${.CURDIR}/../MAKEDEV.common
MAKEDEVSRC+=	MAKEDEV.md

MAKEDEV: ${MAKEDEVSRC}
	@if m4 -I${.CURDIR}/.. -I${.CURDIR} \
	    ${.CURDIR}/../MAKEDEV.mi > MAKEDEV.tmp; then \
	    if ! [ -f ${.CURDIR}/MAKEDEV ]; then \
		echo "    m4 -I.. -I. ../MAKEDEV.mi > MAKEDEV"; \
		cp MAKEDEV.tmp ${.CURDIR}/MAKEDEV; \
		rm MAKEDEV.tmp; \
		exit 0; \
	    fi; \
	    diff MAKEDEV.tmp ${.CURDIR}/MAKEDEV > diffout1 || true; \
	    sed -e 's/^--.*$$//' \
		-e 's/^[\<\>] #.*OpenBSD.*Exp $$//' \
		-e 's/^[0-9]*c[0-9]*$$//' \
		-e '/^$$/d' \
		diffout1 > diffout2; \
	    if [ -s diffout2 ]; then \
		echo "    m4 -I.. -I. ../MAKEDEV.mi > MAKEDEV"; \
	    	cp MAKEDEV.tmp ${.CURDIR}/MAKEDEV || true; \
	    fi; \
	    rm diffout1 diffout2; \
	fi
