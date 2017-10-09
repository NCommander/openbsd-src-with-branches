#	$OpenBSD: Makefile.inc,v 1.7 2014/03/26 18:13:15 eric Exp $

# asr sources
.PATH: ${LIBCSRCDIR}/asr

SRCS+=	asr.c asr_debug.c asr_utils.c \
	res_send_async.c res_search_async.c getrrsetbyname_async.c \
	gethostnamadr_async.c getnetnamadr_async.c \
	getaddrinfo_async.c getnameinfo_async.c

MAN+=	asr_run.3
