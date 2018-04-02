#	$OpenBSD$

AWK=	awk

PROG=	makemap.awk
MAP=	../../../dev/pckbc/wskbdmap_mfii.c

all:	wskbdmap_sgi.c

wskbdmap_sgi.c: ${MAP} ${PROG}
	/bin/rm -f $@
	${AWK} -f ${PROG} ${MAP} > $@
