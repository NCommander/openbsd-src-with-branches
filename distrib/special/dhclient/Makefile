#	$OpenBSD: Makefile,v 1.5 2001/07/30 11:25:14 hugh Exp $

.include <bsd.own.mk>

.PATH:	${.CURDIR}/../../../sbin/dhclient

COPTS+=-Os
CLIENT_PATH='"PATH=/usr/bin:/usr/sbin:/bin:/sbin"'
CPPFLAGS+= -I${.CURDIR}/../../../sbin/dhclient
SRCS=	dhclient.c clparse.c alloc.c dispatch.c hash.c bpf.c options.c \
	tree.c conflex.c errwarn.c inet.c packet.c convert.c tables.c parse.c
PROG=	dhclient
MAN=

LDSTATIC?=      -static

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
