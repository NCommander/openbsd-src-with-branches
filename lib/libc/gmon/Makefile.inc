#	$OpenBSD: Makefile.inc,v 1.4 1998/11/20 11:18:40 d Exp $

# gmon sources
.PATH: ${LIBCSRCDIR}/gmon

SRCS+=	gmon.c mcount.c
MAN+=	moncontrol.3

MLINKS+=moncontrol.3 monstartup.3

# mcount cannot be compiled with profiling or pie
mcount.po:
	@echo "${COMPILE.c} ${NOPIE_FLAGS} ${.IMPSRC} -o ${.TARGET}"
	@${COMPILE.c} ${NOPIE_FLAGS} ${.IMPSRC} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o
