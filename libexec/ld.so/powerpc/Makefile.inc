#	$OpenBSD$

CFLAGS += -fpic -msoft-float
ADDR=-Tdata 8000
ELF_LDFLAGS+=${ADDR}
