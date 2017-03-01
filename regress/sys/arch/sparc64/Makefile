#	$OpenBSD: Makefile,v 1.2 2003/07/12 04:08:33 jason Exp $

.if ${MACHINE_ARCH} == "sparc64"
SUBDIR=emul-popc emul-ldqstq

.elif make(regress) || make(all)
${.TARGETS}:
	@echo SKIPPED
.endif

install:

.include <bsd.subdir.mk>
