# $OpenBSD: Makefile,v 1.1 2018/07/25 18:04:09 jsing Exp $

GO_VERSION != sh -c "(go version) 2>/dev/null || true"

.if empty(GO_VERSION)
regress:
	@echo package go is required for this regress
	@echo SKIPPED
.endif

CLEANFILES+=wycheproof
REGRESS_TARGETS=regress-wycheproof

audit: wycheproof
	./wycheproof -v

wycheproof: wycheproof.go
	go build -o wycheproof ${.CURDIR}/wycheproof.go

regress-wycheproof: wycheproof
	./wycheproof

.PHONY: audit

.include <bsd.regress.mk>
