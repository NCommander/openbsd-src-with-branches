#	$OpenBSD: Makefile,v 1.2 1998/10/13 23:09:52 marc Exp $
LIB=	install
SRCS=	file.c plist.c str.c exec.c global.c pen.c
.PATH:  ${.CURDIR}/../sign
SRCS+= check.c gzip.c common.c sha1.c pgp_check.c pgp_sign.c
CFLAGS+= ${DEBUG} 
NOPROFILE= yes
NOPIC=	yes

install:
	@echo -n

.include <bsd.lib.mk>
