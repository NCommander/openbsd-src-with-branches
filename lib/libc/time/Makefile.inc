#	$OpenBSD: Makefile.inc,v 1.9 2011/10/16 13:22:06 stsp Exp $

.PATH: ${LIBCSRCDIR}/time

SRCS+=	asctime.c difftime.c localtime.c strftime.c strptime.c wcsftime.c
MAN+=	ctime.3 strftime.3 time2posix.3 tzfile.5 tzset.3 strptime.3 wcsftime.3
