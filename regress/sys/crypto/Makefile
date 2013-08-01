#	$OpenBSD: Makefile,v 1.8 2010/09/22 12:04:13 mikeb Exp $

SUBDIR+= cmac
SUBDIR+= gmac
SUBDIR+= hmac
SUBDIR+= key_wrap

.if defined(REGRESS_FULL)
SUBDIR+= enc
SUBDIR+= aesctr
SUBDIR+= aesxts
SUBDIR+= aes
.endif

install:

.include <bsd.subdir.mk>
