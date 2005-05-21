#	$OpenBSD$

AWK=	awk
PROG=	makemap.awk

SOURCE=	../../../dev/pckbc/wskbdmap_mfii.c

gsckbdmap.c: ${SOURCE} ${PROG}
	/bin/rm -f $@
	${AWK} -f ${PROG} ${SOURCE} > $@
