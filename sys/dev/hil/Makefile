#	$OpenBSD$

AWK=	awk
PROG=	devlist2h.awk
SOURCE=	hildevs

${SOURCE}_data.h: ${SOURCE} ${PROG}
	/bin/rm -f $@
	${AWK} -f ${PROG} ${SOURCE} > $@
