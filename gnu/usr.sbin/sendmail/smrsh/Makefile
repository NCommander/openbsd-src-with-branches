#	$OpenBSD: Makefile,v 1.2 2000/02/16 16:51:09 form Exp $

PROG=	smrsh
MAN=	smrsh.8
ENVDEF+=-DNOT_SENDMAIL -DCMDDIR=\"/usr/libexec/sm.bin\"

WANT_LIBSMUTIL=1

.include "../../Makefile.inc"
.include <bsd.prog.mk>
