# $OpenBSD: Makefile,v 1.2 2019/12/02 23:04:49 deraadt Exp $

PROG=	syscall_library
.if ${.CURDIR} == ${.OBJDIR}
LIBDIR=	../libgadgetsyscall
.else
LIBDIR=	${.CURDIR}/../libgadgetsyscall/${__objdir}
.endif
LDADD+=	-L${LIBDIR} -lgadgetsyscall

run-regress-${PROG}: ${PROG}
	rm -f ${PROG}.core
	! LD_LIBRARY_PATH=${LIBDIR} ./${PROG}
	if [ ! -e ${PROG}.core ]; then echo "No coredump"; false; fi

.include <bsd.regress.mk>
