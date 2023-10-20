#	$OpenBSD$

.if ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "powerpc64"
PROG = vecast
CFLAGS += -maltivec
LDADD = -lm

.if ! (make(clean) || make(cleandir) || make(obj))
ALTIVEC !=		/sbin/sysctl -n machdep.altivec
.if ${ALTIVEC} == 0
REGRESS_SKIP_TARGETS =	${REGRESS_TARGETS}
.endif
.endif

.elif make(regress) || make(all)
regress:
	@echo Cannot run on ${MACHINE_ARCH}.
	@echo SKIPPED
.endif
.include <bsd.regress.mk>
