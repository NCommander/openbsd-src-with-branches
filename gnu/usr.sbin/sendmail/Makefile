#	$OpenBSD: Makefile,v 1.5 1998/05/19 16:56:48 millert Exp $

SUBDIR= libsmutil libsmdb sendmail mailstats makemap praliases smrsh cf/cf
#SUBDIR+= libmilter mail.local rmail vacation
.if make(install)
SUBDIR+= doc/op cf
.endif

.include <bsd.subdir.mk>
