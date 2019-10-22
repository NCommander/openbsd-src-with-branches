#	$OpenBSD: Makefile,v 1.1.1.1 2017/10/11 12:01:20 bluhm Exp $

.if ${MACHINE} == "amd64"
SUBDIR =
SUBDIR += config

.elif make(regress) || make(all)
${.TARGETS}:
	# vmd(8) exists only on amd64, but not on ${MACHINE}
	@echo SKIPPED
.endif

install:

.include <bsd.subdir.mk>
