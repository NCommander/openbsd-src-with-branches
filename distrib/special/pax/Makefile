#	$OpenBSD: Makefile,v 1.11 2014/01/08 04:58:36 guenther Exp $

.PATH:  ${.CURDIR}/../../../bin/pax
CFLAGS+=-DNOCPIO -I${.CURDIR}/../../../bin/pax

PROG=   pax
SRCS=	ar_io.c ar_subs.c buf_subs.c cache.c file_subs.c ftree.c\
	gen_subs.c getoldopt.c options.c pat_rep.c pax.c sel_subs.c tables.c\
	tar.c tty_subs.c

.include <bsd.prog.mk>
