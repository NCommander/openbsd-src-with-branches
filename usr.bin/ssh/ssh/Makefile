#	$OpenBSD: Makefile,v 1.48 2003/08/22 10:56:09 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh
BINOWN=	root

#BINMODE?=4555

BINDIR=	/usr/bin
MAN=	ssh.1 ssh_config.5
LINKS=	${BINDIR}/ssh ${BINDIR}/slogin
MLINKS=	ssh.1 slogin.1

SRCS=	ssh.c readconf.c clientloop.c sshtty.c \
	sshconnect.c sshconnect1.c sshconnect2.c

.include <bsd.own.mk> # for AFS

.if (${KERBEROS5:L} == "yes")
CFLAGS+= -DKRB5 -I${DESTDIR}/usr/include/kerberosV -DGSSAPI 
.endif # KERBEROS5

.include <bsd.prog.mk>

.if (${KERBEROS5:L} == "yes")
LDADD+=  -lgssapi -lkrb5
DPADD+=  ${LIBGSSAPI} ${LIBKRB5}
.endif # KERBEROS5

DPADD+=	${LIBGSSAPI} ${LIBKRB5} ${LIBCRYPTO} ${LIBZ} ${LIBDES}
LDADD+=	-lgssapi -lkrb5 -lcrypto -lz -ldes
