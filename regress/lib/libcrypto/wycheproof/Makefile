# $OpenBSD$

GO_VERSION != sh -c "(go version) 2>/dev/null || true"

.if empty(GO_VERSION)
regress:
	@echo package go is required for this regress
	@echo SKIPPED
.endif

CLEANFILES+=wycheproof
REGRESS_TARGETS=regress-wycheproof

regress-wycheproof:
	go build -o wycheproof ${.CURDIR}/wycheproof.go
	./wycheproof

.include <bsd.regress.mk>
