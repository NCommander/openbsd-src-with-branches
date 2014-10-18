#	$OpenBSD: Makefile,v 1.37 2014/10/17 22:59:46 bluhm Exp $

LIB=	event
SRCS=	buffer.c evbuffer.c event.c event_tagging.c evutil.c kqueue.c \
	log.c poll.c select.c signal.c
HDRS=	event.h evutil.h
MAN=	event.3
MLINKS=	event.3 bufferevent_base_set.3 \
	event.3 bufferevent_disable.3 \
	event.3 bufferevent_enable.3 \
	event.3 bufferevent_free.3 \
	event.3 bufferevent_new.3 \
	event.3 bufferevent_read.3 \
	event.3 bufferevent_settimeout.3 \
	event.3 bufferevent_write.3 \
	event.3 bufferevent_write_buffer.3 \
	event.3 evbuffer_add.3 \
	event.3 evbuffer_add_buffer.3 \
	event.3 evbuffer_add_printf.3 \
	event.3 evbuffer_add_vprintf.3 \
	event.3 evbuffer_drain.3 \
	event.3 evbuffer_find.3 \
	event.3 evbuffer_free.3 \
	event.3 evbuffer_new.3 \
	event.3 evbuffer_read.3 \
	event.3 evbuffer_readline.3 \
	event.3 evbuffer_write.3 \
	event.3 event_add.3 \
	event.3 event_base_dispatch.3 \
	event.3 event_base_free.3 \
	event.3 event_base_loop.3 \
	event.3 event_base_loopbreak.3 \
	event.3 event_base_loopexit.3 \
	event.3 event_base_once.3 \
	event.3 event_base_set.3 \
	event.3 event_del.3 \
	event.3 event_dispatch.3 \
	event.3 event_init.3 \
	event.3 event_initialized.3 \
	event.3 event_loop.3 \
	event.3 event_loopbreak.3 \
	event.3 event_loopexit.3 \
	event.3 event_once.3 \
	event.3 event_pending.3 \
	event.3 event_priority_init.3 \
	event.3 event_priority_set.3 \
	event.3 event_set.3 \
	event.3 evtimer_add.3 \
	event.3 evtimer_del.3 \
	event.3 evtimer_initialized.3 \
	event.3 evtimer_pending.3 \
	event.3 evtimer_set.3 \
	event.3 signal_add.3 \
	event.3 signal_del.3 \
	event.3 signal_initialized.3 \
	event.3 signal_pending.3 \
	event.3 signal_set.3 \
	event.3 event_asr_run.3 \
	event.3 event_asr_abort.3

CFLAGS+= -I${.CURDIR} -DNDEBUG

CDIAGFLAGS=	-Wall
#CDIAGFLAGS+=	-Werror
CDIAGFLAGS+=	-Wstrict-prototypes
CDIAGFLAGS+=	-Wmissing-prototypes
CDIAGFLAGS+=	-Wmissing-declarations
CDIAGFLAGS+=	-Wshadow
CDIAGFLAGS+=	-Wpointer-arith
CDIAGFLAGS+=	-Wcast-qual
CDIAGFLAGS+=	-Wsign-compare
CDIAGFLAGS+=	-Wcast-align
CDIAGFLAGS+=	-Wbad-function-cast

includes:
	@cd ${.CURDIR}; for i in ${HDRS}; do \
	  cmp -s $$i ${DESTDIR}/usr/include/$$i || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) $$i \
	  ${DESTDIR}/usr/include; done

.include <bsd.lib.mk>
