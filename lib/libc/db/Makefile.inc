#	$OpenBSD: Makefile.inc,v 1.2 1996/08/19 08:19:54 tholo Exp $
#
CFLAGS+=-D__DBINTERFACE_PRIVATE

.include "${LIBCSRCDIR}/db/btree/Makefile.inc"
.include "${LIBCSRCDIR}/db/db/Makefile.inc"
.include "${LIBCSRCDIR}/db/hash/Makefile.inc"
.include "${LIBCSRCDIR}/db/man/Makefile.inc"
.include "${LIBCSRCDIR}/db/mpool/Makefile.inc"
.include "${LIBCSRCDIR}/db/recno/Makefile.inc"
