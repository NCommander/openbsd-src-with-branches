#	$OpenBSD: Makefile,v 1.2 2000/04/02 19:48:09 millert Exp $

# We only use these libs internally
.if !make(install)
SUBDIR= libsmutil libsmdb
.else
SUBDIR=
.endif

# The bits we install
SUBDIR+=sendmail mailstats makemap praliases smrsh cf/cf

# Extra bits we don't build/install right now
#SUBDIR+= libmilter mail.local rmail vacation

# These just get installed verbatim
.if make(install)
SUBDIR+= doc/op cf
.endif

.include <bsd.subdir.mk>
