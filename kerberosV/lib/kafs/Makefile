# $OpenBSD$

LIB=	kafs5

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

INCLUDE_FILES=	${KRB5SRC}/lib/kafs/kafs.h

SRCS=	afssys.c afskrb.c afskrb5.c common.c $(AIX_SRC) kafs_locl.h afssysdefs.h

.PATH:	${KRB5SRC}/lib/kafs

CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/asn1 -I../asn1 -I../krb5 \
	-I${KRB5SRC}/lib/hdb -I../hdb -I. ${LIBROKEN}

.include <bsd.lib.mk>