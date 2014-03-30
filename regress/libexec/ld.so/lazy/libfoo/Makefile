#	$OpenBSD$

.include <bsd.obj.mk>

LIB=	foo
SRCS=	foo.c

BARDIR!=	if test -d ${.CURDIR}/../libbar/${__objdir}; then \
			echo "${.CURDIR}/../libbar/${__objdir}";  \
		else						  \
			echo "${.CURDIR}/../libbar";		  \
		fi

CPPFLAGS= -DBAR=\"${BARDIR}/libbar.so\"

regress: all

.include <bsd.lib.mk>
