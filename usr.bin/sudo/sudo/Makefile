#	$OpenBSD: Makefile,v 1.7 2009/04/11 11:48:06 millert Exp $

.PATH:		${.CURDIR}/.. ${.CURDIR}/../auth

PROG=	sudo
BINOWN=	root
BINMODE=4555
LINKS=  ${BINDIR}/sudo ${BINDIR}/sudoedit
CFLAGS+=-DSUDO_DEVEL

SRCS=	audit.c check.c env.c getspwuid.c interfaces.c lbuf.c logging.c \
	parse.c pwutil.c set_perms.c sudo.c sudo_edit.c sudo_nss.c term.c \
	tgetpass.c sudo_auth.c bsdauth.c
CPPFLAGS+=	-I..

BINDIR=	/usr/bin

MAN=	sudo.8 sudoers.5
MLINKS= sudo.8 sudoedit.8

CLEANFILES+= ${MAN}

sudo.8: ${.CURDIR}/../varsub sudo.mdoc.in
	sed -f ${.ALLSRC} > $@

sudoers.5: ${.CURDIR}/../varsub sudoers.mdoc.in
	sed -f ${.ALLSRC} > $@

afterdepend: ${MAN}

.include <bsd.prog.mk>
