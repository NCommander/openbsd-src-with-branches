#	$OpenBSD$

PROG=	mrouted
SRCS=	callout.c cfparse.c config.c igmp.c inet.c kern.c main.c prune.c \
	route.c vif.c
MAN=	mrouted.8

CLEANFILES+= cfparse.c y.tab.h

.include <bsd.prog.mk>
