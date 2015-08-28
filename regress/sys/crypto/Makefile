#	$OpenBSD: Makefile,v 1.9 2013/08/01 21:26:30 kettenis Exp $

SUBDIR+= cmac
SUBDIR+= gmac
SUBDIR+= hmac
SUBDIR+= key_wrap

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= enc
SUBDIR+= aesctr
SUBDIR+= aesxts
SUBDIR+= aes
.endif

install:

.include <bsd.subdir.mk>
