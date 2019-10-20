#	$OpenBSD: Makefile.inc,v 1.5 2019/04/19 08:59:40 visa Exp $

# no jump tables in _dl_boot_bind()
CFLAGS += -fno-jump-tables

CHECK_LDSO=c() {							\
	! readelf -Wr $$1 |						\
	  egrep -qv '^($$|[ R])| 0+(1203)? R_MIPS_(NONE|REL32) ';	\
	! readelf -Ws $$1 |						\
	  egrep -qv '^($$|S| *N| *[0-9]+:.*(LOCAL|GLOBAL +DEFAULT +[0-9]+ ))'; \
	}; c

