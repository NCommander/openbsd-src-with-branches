# $OpenBSD$

LIB	= kafs

KRBDEPTH=../..
.include <../../Makefile.inc>

INCLUDE_FILES	= ${KRBSRC}/lib/kafs/kafs.h

CFLAGS += -I${KRBSRC}/lib/kafs

.PATH:	${KRBSRC}/lib/kafs

SRCS=	afssys.c \
	afskrb.c \
	$(afskrb5_c) \
	common.c \
	kafs_locl.h \
	afssysdefs.h

.include <bsd.lib.mk>
