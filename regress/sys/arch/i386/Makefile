#	$OpenBSD$

.if ${MACHINE_ARCH} == "i386"
SUBDIR =
SUBDIR +=	f00f

.elif make(regress) || make(all)
${.TARGETS}:
	@echo Regress for architecture i386 cannot run on ${MACHINE_ARCH}.
	@echo SKIPPED
.endif

install:

.include <bsd.subdir.mk>
