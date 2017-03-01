#       $OpenBSD: Makefile,v 1.2 2004/05/13 01:22:05 mickey Exp $

.if ${MACHINE_ARCH} == "hppa"
SUBDIR+= probe sfuid

.elif make(regress) || make(all)
${.TARGETS}:
	@echo SKIPPED
.endif

install:

.include <bsd.subdir.mk>
