#       $OpenBSD: Makefile,v 1.3 2017/03/01 00:40:39 bluhm Exp $

.if ${MACHINE_ARCH} == "hppa"
SUBDIR+= probe sfuid

.elif make(regress) || make(all)
${.TARGETS}:
	@echo Regress for architecture hppa cannot run on ${MACHINE_ARCH}.
	@echo SKIPPED
.endif

install:

.include <bsd.subdir.mk>
