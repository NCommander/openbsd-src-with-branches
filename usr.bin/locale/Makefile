#	$OpenBSD$

# for LOCALECHARSETS
.include "${.CURDIR}/../../share/locale/ctype/Makefile.inc"

PROG=	locale
CFLAGS+=	-DLOCALE_CHARSETS="\"${LOCALECHARSETS}\""

.include <bsd.prog.mk>
