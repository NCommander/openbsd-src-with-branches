#	$OpenBSD: Makefile,v 1.2 1996/09/25 06:40:52 etheisen Exp $

PROG=		crunchgen
MAN=		crunchgen.1 crunchide.1
SRCS=		crunchgen.c crunched_skel.c \
		crunchide.c elf_hide.c ecoff_hide.c
CFLAGS+=	-g -Wall
CLEANFILES+=	crunched_skel.c

crunched_skel.c: crunched_main.c
	sh ${.CURDIR}/mkskel.sh ${.CURDIR}/crunched_main.c > crunched_skel.c

LINKS=  ${BINDIR}/crunchgen ${BINDIR}/crunchide

.include <bsd.prog.mk>
