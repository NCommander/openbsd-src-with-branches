#	$OpenBSD: Makefile,v 1.4 1997/04/27 20:55:51 millert Exp $

CFLAGS += -I${.CURDIR} -DNO_UNDERSCORE -DVERBOSE_DLINKER \
	-DUSE_CACHE -D__PIC__ -I${.CURDIR}/${MACHINE_ARCH}

SHLIB_MAJOR=1
SHLIB_MINOR=0

SRCS=	libdl.c
LIB=	dl

NOPROFILE=yes

BINDIR=/usr/libexec
INSTALL_STRIP=



.include <bsd.lib.mk>
