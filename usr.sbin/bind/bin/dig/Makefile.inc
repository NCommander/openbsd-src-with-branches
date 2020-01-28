#	$OpenBSD$

.PATH: ${.CURDIR}/bin/dig

SRCS+=	dig.c dighost.c host.c nslookup.c
MAN+=	dig.1 host.1 nslookup.1
