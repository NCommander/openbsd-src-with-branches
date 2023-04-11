#	$OpenBSD$

.if ! (make(clean) || make(cleandir) || make(obj))
GO_VERSION != sh -c "(go version) 2>/dev/null || true"
.endif

.if empty(GO_VERSION)
regress:
	@echo package go is required for this regress
	@echo SKIPPED
.endif

REGRESS_TARGETS=regress-go-verify

certs:
	cd ${.CURDIR} && sh ./make-certs.sh

regress-go-verify:
	cd ${.CURDIR} && go test -test.v .

.include <bsd.regress.mk>
