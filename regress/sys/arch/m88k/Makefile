#	$OpenBSD: Makefile,v 1.3 2017/03/01 00:40:39 bluhm Exp $

.if ${MACHINE_ARCH} == "m88k"
SUBDIR+= usertrap

.elif make(regress) || make(all)
${.TARGETS}:
	@echo Regress for architecture m88k cannot run on ${MACHINE_ARCH}.
	@echo SKIPPED
.endif

install:

.include <bsd.subdir.mk>
