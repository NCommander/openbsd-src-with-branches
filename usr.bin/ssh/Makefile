#	$OpenBSD: Makefile,v 1.3 1999/09/26 22:29:50 deraadt Exp $

.include <bsd.own.mk>

SUBDIR=	ssh sshd ssh-add ssh-keygen ssh-agent scp

distribution:
	install -C -o root -g wheel -m 0644 ${.CURDIR}/ssh_config \
	    ${DESTDIR}/etc/ssh_config
	install -C -o root -g wheel -m 0644 ${.CURDIR}/sshd_config \
	    ${DESTDIR}/etc/sshd_config

.include <bsd.subdir.mk>
