# $OpenBSD: Makefile,v 1.1.1.1 2005/09/13 20:51:39 drahn Exp $

.include <bsd.obj.mk>

PROG=prog4

SRCS=main.C

AA_DIR=${.CURDIR}/../libaa
AA_OBJDIR!=    if [ -d $(AA_DIR)/${__objdir} ]; then \
                        echo "$(AA_DIR)/${__objdir}"; \
		else \
			echo "$(AA_DIR)"; \
		fi

AC_DIR=${.CURDIR}/../libac
AC_OBJDIR!=    if [ -d $(AC_DIR)/${__objdir} ]; then \
                        echo "$(AC_DIR)/${__objdir}"; \
		else \
			echo "$(AC_DIR)"; \
		fi



CFLAGS=-I${.CURDIR}/../libac
LDADD=
#LDADD+=-laa
#LDADD+=-lac
LDFLAGS=-L$(AA_OBJDIR) -L$(AC_OBJDIR)
#LDFLAGS+= -Wl,-rpath,$(AA_OBJDIR)  -Wl,-rpath,$(AC_OBJDIR) 
# This intentionally leaves out AA_OBJDIR from -rpath
LDFLAGS+= -Wl,-rpath,$(AC_OBJDIR) 
NOMAN=
CC=c++

.include <bsd.regress.mk>
