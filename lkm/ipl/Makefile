#	$OpenBSD$

LKM=	ipl
SRCS+=	stub.c ip_fil.c fil.c ip_nat.c ip_frag.c ip_state.c
CFLAGS+= -DIPFILTER_LKM
NOMAN=	noman

.PATH: ${.CURDIR}/../../sys/netinet

.include <bsd.lkm.mk>
