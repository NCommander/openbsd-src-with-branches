#	$OpenBSD: Makefile,v 1.3 2000/04/11 07:31:32 form Exp $

PROG=	smrsh
MAN=	smrsh.8
ENVDEF=	-DNOT_SENDMAIL -DCMDDIR=\"/usr/libexec/sm.bin\"

WANT_LIBSMUTIL=1

.include "../../../libexec/Makefile.inc"
.include <bsd.prog.mk>
