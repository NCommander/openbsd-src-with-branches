#	$OpenBSD$

SEQ?=	/usr/bin/seq

seqtest:
	sh ${.CURDIR}/$@.sh ${SEQ} $@.out
	diff ${.CURDIR}/$@.expected $@.out
