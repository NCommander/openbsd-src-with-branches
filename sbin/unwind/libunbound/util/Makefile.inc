#	$OpenBSD: Makefile.inc,v 1.2 2019/01/25 12:15:49 florian Exp $

.PATH: ${.CURDIR}/libunbound/util

SRCS+=	alloc.c as112.c config_file.c configlexer.c configparser.y edns.c \
	fptr_wlist.c locks.c mini_event.c module.c net_help.c netevent.c \
	random.c rbtree.c regional.c rtt.c tcp_conn_limit.c timehist.c tube.c \
	ub_event.c util_log.c winsock_event.c

util_log.c:
	ln -s ${.CURDIR}/libunbound/util/log.c $@

CLEANFILES+=	util_log.c
