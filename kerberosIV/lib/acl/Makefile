# $OpenBSD$

LIB	= acl

KRBDEPTH=../..
.include <../../Makefile.inc>

INCLUDE_FILES	= ${KRBSRC}/lib/acl/acl.h

.PATH:	${KRBSRC}/lib/acl

CFLAGS	+= -I${KRBSRC}/lib/acl

SRCS=	acl_files.c

.include <bsd.lib.mk>