#	$OpenBSD: Makefile.inc,v 1.2 1998/08/22 07:39:52 smurph Exp $

LIB_PROM_DIR=${S}/arch/aviion/stand/libprom

LIBPROM_DIR!= cd ${LIB_PROM_DIR}; \
     printf "xxx:\n\techo \$${.OBJDIR}\n" | ${MAKE} -r -s -f - xxx

LIBPROM=${LIBPROM_DIR}/libprom.a

$(LIBPROM):	.NOTMAIN __always_make_libprom
	@echo making sure the libprom is up to date...
	@(cd ${LIB_PROM_DIR}; ${MAKE})

__always_make_libprom:	.NOTMAIN
