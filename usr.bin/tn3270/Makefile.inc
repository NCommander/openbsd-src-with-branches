#	$OpenBSD: Makefile.inc,v 1.3 1998/07/27 15:24:32 millert Exp $

CFLAGS += -DSRCRT -DKLUDGELINEMODE -DUSE_TERMIO -DTN3270 -Dunix
KBD = unix.kbd

.include "${.CURDIR}/../../Makefile.inc"
