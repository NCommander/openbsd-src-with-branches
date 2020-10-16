# $OpenBSD$

.if ${MACHINE} != "amd64"
regress:
	# Need syscall asm for this arch
	@echo SKIPPED
.endif
