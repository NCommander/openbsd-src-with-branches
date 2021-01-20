/*	$OpenBSD: rde_decide_test.c,v 1.1 2021/01/19 16:04:46 claudio Exp $ */

/*
 * Copyright (c) 2020 Claudio Jeker <claudio@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/types.h>
#include <sys/queue.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rde.h"

struct rde_memstats rdemem;

struct rib dummy_rib = {
	.name = "regress RIB",
	.flags = 0,
};

struct rib_entry dummy_re;

struct nexthop nh_reach = {
	.state = NEXTHOP_REACH
};
struct nexthop nh_unreach = {
	.state = NEXTHOP_UNREACH
};

struct rde_peer peer1 = {
	.conf.ebgp = 1,
	.remote_bgpid = 1,
	.remote_addr = { .aid = AID_INET, .v4.s_addr = 0xef000001 },
};
struct rde_peer peer2 = {
	.conf.ebgp = 1,
	.remote_bgpid = 2,
	.remote_addr = { .aid = AID_INET, .v4.s_addr = 0xef000002 },
};
struct rde_peer peer3 = {
	.conf.ebgp = 0,
	.remote_bgpid = 3,
	.remote_addr = { .aid = AID_INET, .v4.s_addr = 0xef000003 },
};
struct rde_peer peer4 = {
	.conf.ebgp = 1,
	.remote_bgpid = 1,
	.remote_addr = { .aid = AID_INET, .v4.s_addr = 0xef000004 },
};

union a {
	struct aspath	a;
	struct {
		LIST_ENTRY(aspath) entry;
		u_int32_t source_as;
		int refcnt;
		uint16_t len;
		uint16_t ascnt;
		uint8_t	d[6];
	} x;
} asdata[] = {
	{ .x = { .len = 6, .ascnt = 2, .d = { 2, 1, 0, 0, 0, 1 } } },
	{ .x = { .len = 6, .ascnt = 3, .d = { 2, 1, 0, 0, 0, 1 } } },
	{ .x = { .len = 6, .ascnt = 2, .d = { 2, 1, 0, 0, 0, 2 } } },
	{ .x = { .len = 6, .ascnt = 3, .d = { 2, 1, 0, 0, 0, 2 } } },
};

struct rde_aspath asp[] = {
	{ .aspath = &asdata[0].a, .med = 100, .lpref = 100, .origin = ORIGIN_IGP, .weight = 1000 },
	/* 1 & 2: errors and loops */
	{ .aspath = &asdata[0].a, .med = 100, .lpref = 100, .origin = ORIGIN_IGP, .flags=F_ATTR_PARSE_ERR },
	{ .aspath = &asdata[0].a, .med = 100, .lpref = 100, .origin = ORIGIN_IGP, .flags=F_ATTR_LOOP },
	/* 3: local preference */
	{ .aspath = &asdata[0].a, .med = 100, .lpref = 50, .origin = ORIGIN_IGP },
	/* 4: aspath count */
	{ .aspath = &asdata[1].a, .med = 100, .lpref = 100, .origin = ORIGIN_IGP },
	/* 5 & 6: origin */
	{ .aspath = &asdata[0].a, .med = 100, .lpref = 100, .origin = ORIGIN_EGP },
	{ .aspath = &asdata[0].a, .med = 100, .lpref = 100, .origin = ORIGIN_INCOMPLETE },
	/* 7: MED */
	{ .aspath = &asdata[0].a, .med = 200, .lpref = 100, .origin = ORIGIN_IGP },
	/* 8: Weight */
	{ .aspath = &asdata[0].a, .med = 100, .lpref = 100, .origin = ORIGIN_IGP, .weight = 100 },
};

#define T1	1610980000
#define T2	1610983600

struct test {
	char *what;
	struct prefix p;
} test_pfx[] = {
	{ .what = "test prefix",
	.p = { .re = &dummy_re, .aspath = &asp[0], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T1, } },
	/* pathes with errors are not eligible */
	{ .what = "prefix with error",
	.p = { .re = &dummy_re, .aspath = &asp[1], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T1, } },
	/* only loop free pathes are eligible */
	{ .what = "prefix with loop",
	.p = { .re = &dummy_re, .aspath = &asp[2], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T1, } },
	/* 1. check if prefix is eligible a.k.a reachable */
	{ .what = "prefix with unreachable nexthop",
	.p = { .re = &dummy_re, .aspath = &asp[0], .peer = &peer1, .nexthop = &nh_unreach, .lastchange = T1, } },
	/* 2. local preference of prefix, bigger is better */
	{ .what = "local preference check",
	.p = { .re = &dummy_re, .aspath = &asp[3], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T1, } },
	/* 3. aspath count, the shorter the better */
	{ .what = "aspath count check",
	.p = { .re = &dummy_re, .aspath = &asp[4], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T1, } },
	/* 4. origin, the lower the better */
	{ .what = "origin EGP",
	.p = { .re = &dummy_re, .aspath = &asp[5], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T1, } },
	{ .what = "origin INCOMPLETE",
	.p = { .re = &dummy_re, .aspath = &asp[6], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T1, } },
	/* 5. MED decision */
	{ .what = "MED",
	.p = { .re = &dummy_re, .aspath = &asp[7], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T1, } },
	/* 6. EBGP is cooler than IBGP */
	{ .what = "EBGP vs IBGP",
	.p = { .re = &dummy_re, .aspath = &asp[0], .peer = &peer3, .nexthop = &nh_reach, .lastchange = T1, } },
	/* 7. weight */
	{ .what = "local weight",
	.p = { .re = &dummy_re, .aspath = &asp[8], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T1, } },
	/* 8. nexthop cost not implemented */
	/* 9. route age */
	{ .what = "route age",
	.p = { .re = &dummy_re, .aspath = &asp[0], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T2, } },
	/* 10. BGP Id or ORIGINATOR_ID if present */
	{ .what = "BGP ID",
	.p = { .re = &dummy_re, .aspath = &asp[0], .peer = &peer2, .nexthop = &nh_reach, .lastchange = T1, } },
	/* 11. CLUSTER_LIST length, TODO */
	/* 12. lowest peer address wins */
	{ .what = "remote peer address",
	.p = { .re = &dummy_re, .aspath = &asp[0], .peer = &peer4, .nexthop = &nh_reach, .lastchange = T1, } },
};

struct rde_aspath med_asp[] = {
	{ .aspath = &asdata[0].a, .med = 100, .lpref = 100, .origin = ORIGIN_EGP },
	{ .aspath = &asdata[0].a, .med = 150, .lpref = 100, .origin = ORIGIN_EGP },
	{ .aspath = &asdata[2].a, .med = 75,  .lpref = 100, .origin = ORIGIN_EGP },
	{ .aspath = &asdata[2].a, .med = 125, .lpref = 100, .origin = ORIGIN_EGP },
};

struct prefix med_pfx1 = 
	{ .re = &dummy_re, .aspath = &med_asp[0], .peer = &peer2, .nexthop = &nh_reach, .lastchange = T1, };
struct prefix med_pfx2 = 
	{ .re = &dummy_re, .aspath = &med_asp[1], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T1, };
struct prefix med_pfx3 = 
	{ .re = &dummy_re, .aspath = &med_asp[2], .peer = &peer3, .nexthop = &nh_reach, .lastchange = T1, };
struct prefix med_pfx4 = 
	{ .re = &dummy_re, .aspath = &med_asp[3], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T1, };

struct prefix age_pfx1 = 
	{ .re = &dummy_re, .aspath = &asp[0], .peer = &peer2, .nexthop = &nh_reach, .lastchange = T1, };
struct prefix age_pfx2 = 
	{ .re = &dummy_re, .aspath = &asp[0], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T2, };

int     prefix_cmp(struct prefix *, struct prefix *);

int	decision_flags = BGPD_FLAG_DECISION_ROUTEAGE;

int	failed;

void
test(struct prefix *a, struct prefix *b)
{
	if (prefix_cmp(a, b) < 0) {
		printf(" FAILED\n");
		failed = 1;
	} else if (prefix_cmp(b, a) > 0) {
		printf(" reverse cmp FAILED\n");
		failed = 1;
	} else
		printf(" OK\n");
}

int
main(int argc, char **argv)
{
	size_t i, ntest;;

	ntest = sizeof(test_pfx) / sizeof(*test_pfx);
	for (i = 1; i < ntest; i++) {
		printf("test %zu: %s", i, test_pfx[i].what);
		test(&test_pfx[0].p, &test_pfx[i].p);
	}

	printf("test NULL element");
	test(&test_pfx[0].p, NULL);

	printf("test strict med 1");
	test(&med_pfx1, &med_pfx2);
	printf("test strict med 2");
	test(&med_pfx1, &med_pfx3);
	printf("test strict med 3");
	test(&med_pfx4, &med_pfx1);

	decision_flags |= BGPD_FLAG_DECISION_MED_ALWAYS;
	printf("test always med 1");
	test(&med_pfx1, &med_pfx2);
	printf("test always med 2");
	test(&med_pfx3, &med_pfx1);
	printf("test always med 3");
	test(&med_pfx1, &med_pfx4);

	printf("test route-age evaluate");
	test(&age_pfx1, &age_pfx2);
	decision_flags &= ~BGPD_FLAG_DECISION_ROUTEAGE;
	printf("test route-age ignore");
	test(&age_pfx2, &age_pfx1);


	if (failed)
		printf("some tests FAILED\n");
	else
		printf("all tests OK\n");
	exit(failed);
}

int
rde_decisionflags(void)
{
	return decision_flags;
}

u_int32_t
rde_local_as(void)
{
	return 65000;
}

int
as_set_match(const struct as_set *aset, u_int32_t asnum)
{
	errx(1, __func__);
}

struct rib *
rib_byid(u_int16_t id)
{
	return &dummy_rib;
}

void
rde_generate_updates(struct rib *rib, struct prefix *new, struct prefix *old)
{
	/* maybe we want to do something here */
}

__dead void
fatalx(const char *emsg, ...)
{
	va_list ap;
	va_start(ap, emsg);
	verrx(2, emsg, ap);
}

__dead void
fatal(const char *emsg, ...)
{
	va_list ap;
	va_start(ap, emsg);
	verr(2, emsg, ap);
}

void
log_warnx(const char *emsg, ...)
{
	va_list  ap;
	va_start(ap, emsg);
	vwarnx(emsg, ap);
	va_end(ap);
}

