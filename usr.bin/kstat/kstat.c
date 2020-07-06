/* $OpenBSD$ */

/*
 * Copyright (c) 2020 David Gwynne <dlg@openbsd.org>
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <inttypes.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>
#include <vis.h>

#include <sys/tree.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <sys/kstat.h>

#ifndef roundup
#define roundup(x, y)   ((((x)+((y)-1))/(y))*(y))
#endif

#define DEV_KSTAT "/dev/kstat"

static void	kstat_list(int, unsigned int);

#if 0
__dead static void
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: %s\n", __progname);
	exit(1);
}
#endif

int
main(int argc, char *argv[])
{
	unsigned int version;
	int fd;

	fd = open(DEV_KSTAT, O_RDONLY);
	if (fd == -1)
		err(1, "%s", DEV_KSTAT);

	if (ioctl(fd, KSTATIOC_VERSION, &version) == -1)
		err(1, "kstat version");

	kstat_list(fd, version);

	return (0);
}

struct kstat_entry {
	struct kstat_req	kstat;
	RBT_ENTRY(kstat_entry)	entry;
	int			serrno;
};

RBT_HEAD(kstat_tree, kstat_entry);

static inline int
kstat_cmp(const struct kstat_entry *ea, const struct kstat_entry *eb)
{
	const struct kstat_req *a = &ea->kstat;
	const struct kstat_req *b = &eb->kstat;
	int rv;

	rv = strncmp(a->ks_provider, b->ks_provider, sizeof(a->ks_provider));
	if (rv != 0)
		return (rv);
	if (a->ks_instance > b->ks_instance)
		return (1);
	if (a->ks_instance < b->ks_instance)
		return (-1);

	rv = strncmp(a->ks_name, b->ks_name, sizeof(a->ks_name));
	if (rv != 0)
		return (rv);
	if (a->ks_unit > b->ks_unit)
		return (1);
	if (a->ks_unit < b->ks_unit)
		return (-1);

	return (0);
}

RBT_PROTOTYPE(kstat_tree, kstat_entry, entry, kstat_cmp);
RBT_GENERATE(kstat_tree, kstat_entry, entry, kstat_cmp);

static int
printable(int ch)
{
	if (ch == '\0')
		return ('_');
	if (!isprint(ch))
		return ('~');
	return (ch);
}

static void
hexdump(const void *d, size_t datalen)
{
	const uint8_t *data = d;
	size_t i, j = 0;

	for (i = 0; i < datalen; i += j) {
		printf("%4zu: ", i);

		for (j = 0; j < 16 && i+j < datalen; j++)
			printf("%02x ", data[i + j]);
		while (j++ < 16)
			printf("   ");
		printf("|");

		for (j = 0; j < 16 && i+j < datalen; j++)
			putchar(printable(data[i + j]));
		printf("|\n");
	}
}

static void
strdump(const void *s, size_t len)
{
	const char *str = s;
	char dst[8];
	size_t i;

	for (i = 0; i < len; i++) {
		char ch = str[i];
		if (ch == '\0')
			break;

		vis(dst, ch, VIS_TAB | VIS_NL, 0);
		printf("%s", dst);
	}
}

static void
strdumpnl(const void *s, size_t len)
{
	strdump(s, len);
	printf("\n");
}

static void
kstat_kv(const void *d, ssize_t len)
{
	const uint8_t *buf;
	const struct kstat_kv *kv;
	ssize_t blen;
	void (*trailer)(const void *, size_t);
	double f;

	if (len < (ssize_t)sizeof(*kv)) {
		warn("short kv (len %zu < size %zu)", len, sizeof(*kv));
		return;
	}

	buf = d;
	do {
		kv = (const struct kstat_kv *)buf;

		buf += sizeof(*kv);
		len -= sizeof(*kv);

		blen = 0;
		trailer = hexdump;

		printf("%16.16s: ", kv->kv_key);

		switch (kv->kv_type) {
		case KSTAT_KV_T_NULL:
			printf("null");
			break;
		case KSTAT_KV_T_BOOL:
			printf("%s", kstat_kv_bool(kv) ? "true" : "false");
			break;
		case KSTAT_KV_T_COUNTER64:
		case KSTAT_KV_T_UINT64:
			printf("%" PRIu64, kstat_kv_u64(kv));
			break;
		case KSTAT_KV_T_INT64:
			printf("%" PRId64, kstat_kv_s64(kv));
			break;
		case KSTAT_KV_T_COUNTER32:
		case KSTAT_KV_T_UINT32:
			printf("%" PRIu32, kstat_kv_u32(kv));
			break;
		case KSTAT_KV_T_INT32:
			printf("%" PRId32, kstat_kv_s32(kv));
			break;
		case KSTAT_KV_T_STR:
			blen = kstat_kv_len(kv);
			trailer = strdumpnl;
			break;
		case KSTAT_KV_T_BYTES:
			blen = kstat_kv_len(kv);
			trailer = hexdump;

			printf("\n");
			break;

		case KSTAT_KV_T_ISTR:
			strdump(kstat_kv_istr(kv), sizeof(kstat_kv_istr(kv)));
			break;

		case KSTAT_KV_T_TEMP:
			f = kstat_kv_temp(kv);
			printf("%.2f degC", (f - 273150000.0) / 1000000.0);
			break;

		default:
			printf("unknown type %u, stopping\n", kv->kv_type);
			return;
		}

		switch (kv->kv_unit) {
		case KSTAT_KV_U_NONE:
			break;
		case KSTAT_KV_U_PACKETS:
			printf(" packets");
			break;
		case KSTAT_KV_U_BYTES:
			printf(" bytes");
			break;
		case KSTAT_KV_U_CYCLES:
			printf(" cycles");
			break;

		default:
			printf(" unit-type-%u", kv->kv_unit);
			break;
		}

		if (blen > 0) {
			if (blen > len) {
				blen = len;
			}

			(*trailer)(buf, blen);
		} else
			printf("\n");

		blen = roundup(blen, KSTAT_KV_ALIGN);
		buf += blen;
		len -= blen;
	} while (len >= (ssize_t)sizeof(*kv));
}

static void
kstat_list(int fd, unsigned int version)
{
	struct kstat_entry *kse;
	struct kstat_req *ksreq;
	size_t len;
	uint64_t id = 0;
	struct kstat_tree kstat_tree = RBT_INITIALIZER();

	for (;;) {
		kse = malloc(sizeof(*kse));
		if (kse == NULL)
			err(1, NULL);

		memset(kse, 0, sizeof(*kse));
		ksreq = &kse->kstat;
		ksreq->ks_version = version;
		ksreq->ks_id = ++id;

		ksreq->ks_datalen = len = 64; /* magic */
		ksreq->ks_data = malloc(len);
		if (ksreq->ks_data == NULL)
			err(1, "data alloc");

		if (ioctl(fd, KSTATIOC_NFIND_ID, ksreq) == -1) {
			if (errno == ENOENT) {
				free(ksreq->ks_data);
				free(kse);
				break;
			}

			kse->serrno = errno;
			goto next;
		}

		while (ksreq->ks_datalen > len) {
			len = ksreq->ks_datalen;
			ksreq->ks_data = realloc(ksreq->ks_data, len);
			if (ksreq->ks_data == NULL)
				err(1, "data resize (%zu)", len);

			if (ioctl(fd, KSTATIOC_FIND_ID, ksreq) == -1)
				err(1, "find id %llu", id);
		}

next:
		if (RBT_INSERT(kstat_tree, &kstat_tree, kse) != NULL)
			errx(1, "duplicate kstat entry");

		id = ksreq->ks_id;
	}

	RBT_FOREACH(kse, kstat_tree, &kstat_tree) {
		ksreq = &kse->kstat;
		printf("%s:%u:%s:%u\n",
		    ksreq->ks_provider, ksreq->ks_instance,
		    ksreq->ks_name, ksreq->ks_unit);
		if (kse->serrno != 0) {
			printf("\t%s\n", strerror(kse->serrno));
			continue;
		}
		switch (ksreq->ks_type) {
		case KSTAT_T_RAW:
			hexdump(ksreq->ks_data, ksreq->ks_datalen);
			break;
		case KSTAT_T_KV:
			kstat_kv(ksreq->ks_data, ksreq->ks_datalen);
			break;
		default:
			hexdump(ksreq->ks_data, ksreq->ks_datalen);
			break;
		}
	}
}
