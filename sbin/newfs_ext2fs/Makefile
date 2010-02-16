# $OpenBSD$
PROG=	newfs_ext2fs
SRCS=	newfs_ext2fs.c mke2fs.c ext2fs_bswap.c
MAN=	newfs_ext2fs.8

DPADD+= ${LIBUTIL}
LDADD+= -lutil

.PATH:  ${.CURDIR}/../../sys/ufs/ext2fs

.include <bsd.prog.mk>
