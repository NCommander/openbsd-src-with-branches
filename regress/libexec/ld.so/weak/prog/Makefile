# $OpenBSD$

.include <bsd.obj.mk>

PROG=	prog
SRCS=	main.c

LIBWEAK!=	if test -d ${.CURDIR}/../libweak/${__objdir} ; then	\
			echo "${.CURDIR}/../libweak/${__objdir}" ;	\
		else							\
			echo "${.CURDIR}/../libweak" ;			\
		fi

LIBSTRONG!=	if test -d ${.CURDIR}/../libstrong/${__objdir} ; then	\
			echo "${.CURDIR}/../libstrong/${__objdir}" ;	\
		else							\
			echo "${.CURDIR}/../libstrong" ;		\
		fi

LDADD+= 	-Wl,-rpath,${LIBWEAK} -L${LIBWEAK} -lweak
LDADD+=		-Wl,-rpath,${LIBSTRONG} -L${LIBSTRONG} -lstrong

.include <bsd.regress.mk>
