# $OpenBSD$

.include <bsd.obj.mk>

PROG=	prog2
SRCS=	main.c

LIBWEAK!=	if test -d ${.CURDIR}/../libweak/${__objdir} ; then	\
			echo "${.CURDIR}/../libweak/${__objdir}" ;	\
		else							\
			echo "${.CURDIR}/../libweak" ;			\
		fi

LDADD+= 	-Wl,-rpath,${LIBWEAK} -L${LIBWEAK} -lweak

.include <bsd.regress.mk>
