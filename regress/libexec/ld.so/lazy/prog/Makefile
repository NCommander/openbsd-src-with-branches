#	$OpenBSD$

.include <bsd.obj.mk>

PROG= prog

FOODIR!=	if test -d ${.CURDIR}/../libfoo/${__objdir}; then \
			echo "${.CURDIR}/../libfoo/${__objdir}";  \
		else						  \
			echo "${.CURDIR}/../libfoo";		  \
		fi

CPPFLAGS= -DFOO=\"${FOODIR}/libfoo.so\"

.include <bsd.regress.mk>
