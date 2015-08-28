#	$OpenBSD: Makefile,v 1.27 2014/10/31 13:48:21 jsing Exp $

# Define SMALL to disable command line editing and https support
#CFLAGS+=-DSMALL

PROG=	ftp
SRCS=	cmds.c cmdtab.c complete.c cookie.c domacro.c fetch.c ftp.c \
	list.c main.c ruserpass.c small.c stringlist.c util.c

CPPFLAGS+= -DINET6

LDADD+=	-ledit -lcurses -lutil -ltls -lssl -lcrypto
DPADD+=	${LIBEDIT} ${LIBCURSES} ${LIBUTIL} ${LIBTLS} ${LIBSSL} ${LIBCRYPTO}
LDSTATIC= ${STATIC}

#COPTS+= -Wall -Wconversion -Wstrict-prototypes -Wmissing-prototypes

.include <bsd.prog.mk>
