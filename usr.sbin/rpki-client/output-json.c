/*	$OpenBSD: output-json.c,v 1.35 2023/04/26 18:34:40 job Exp $ */
/*
 * Copyright (c) 2019 Claudio Jeker <claudio@openbsd.org>
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

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <netdb.h>

#include "extern.h"
#include "json.h"

static void
outputheader_json(struct stats *st)
{
	char		 hn[NI_MAXHOST], tbuf[26];
	struct tm	*tp;
	time_t		 t;
	int		 i;

	time(&t);
	tp = gmtime(&t);
	strftime(tbuf, sizeof tbuf, "%FT%TZ", tp);

	gethostname(hn, sizeof hn);

	json_do_object("metadata");

	json_do_printf("buildmachine", "%s", hn);
	json_do_printf("buildtime", "%s", tbuf);
	json_do_int("elapsedtime", st->elapsed_time.tv_sec);
	json_do_int("usertime", st->user_time.tv_sec);
	json_do_int("systemtime", st->system_time.tv_sec);
	json_do_int("roas", st->repo_tal_stats.roas);
	json_do_int("failedroas", st->repo_tal_stats.roas_fail);
	json_do_int("invalidroas", st->repo_tal_stats.roas_invalid);
	json_do_int("aspas", st->repo_tal_stats.aspas);
	json_do_int("failedaspas", st->repo_tal_stats.aspas_fail);
	json_do_int("invalidaspas", st->repo_tal_stats.aspas_invalid);
	json_do_int("bgpsec_pubkeys", st->repo_tal_stats.brks);
	json_do_int("certificates", st->repo_tal_stats.certs);
	json_do_int("invalidcertificates", st->repo_tal_stats.certs_fail);
	json_do_int("taks", st->repo_tal_stats.taks);
	json_do_int("tals", st->tals);
	json_do_int("invalidtals", talsz - st->tals);

	json_do_array("talfiles");
	for (i = 0; i < talsz; i++)
		json_do_printf("name", "%s", tals[i]);
	json_do_end();

	json_do_int("manifests", st->repo_tal_stats.mfts);
	json_do_int("failedmanifests", st->repo_tal_stats.mfts_fail);
	json_do_int("stalemanifests", st->repo_tal_stats.mfts_stale);
	json_do_int("crls", st->repo_tal_stats.crls);
	json_do_int("gbrs", st->repo_tal_stats.gbrs);
	json_do_int("repositories", st->repos);
	json_do_int("vrps", st->repo_tal_stats.vrps);
	json_do_int("uniquevrps", st->repo_tal_stats.vrps_uniqs);
	json_do_int("vaps", st->repo_tal_stats.vaps);
	json_do_int("uniquevaps", st->repo_tal_stats.vaps_uniqs);
	json_do_int("cachedir_del_files", st->repo_stats.del_files);
	json_do_int("cachedir_superfluous_files", st->repo_stats.extra_files);
	json_do_int("cachedir_del_dirs", st->repo_stats.del_dirs);

	json_do_end();
}

static void
print_vap(struct vap *v, enum afi afi)
{
	size_t i;
	int found = 0;

	json_do_object("aspa");
	json_do_int("customer_asid", v->custasid);
	json_do_int("expires", v->expires);

	json_do_array("providers");
	for (i = 0; i < v->providersz; i++) {
		if (v->providers[i].afi != 0 && v->providers[i].afi != afi)
			continue;
		found = 1;
		json_do_int("provider", v->providers[i].as);
	}
	if (!found)
		json_do_int("provider", 0);
	json_do_end();
}

static void
output_aspa(struct vap_tree *vaps)
{
	struct vap	*v;

	json_do_object("provider_authorizations");

	json_do_array("ipv4");
	RB_FOREACH(v, vap_tree, vaps) {
		print_vap(v, AFI_IPV4);
	}
	json_do_end();

	json_do_array("ipv6");
	RB_FOREACH(v, vap_tree, vaps) {
		print_vap(v, AFI_IPV6);
	}
	json_do_end();
}

int
output_json(FILE *out, struct vrp_tree *vrps, struct brk_tree *brks,
    struct vap_tree *vaps, struct stats *st)
{
	char		 buf[64];
	struct vrp	*v;
	struct brk	*b;

	json_do_start(out);
	outputheader_json(st);

	json_do_array("roas");
	RB_FOREACH(v, vrp_tree, vrps) {
		ip_addr_print(&v->addr, v->afi, buf, sizeof(buf));

		json_do_object("roa");
		json_do_int("asn", v->asid);
		json_do_printf("prefix", "%s", buf);
		json_do_int("maxLength", v->maxlength);
		json_do_printf("ta", "%s", taldescs[v->talid]);
		json_do_int("expires", v->expires);
		json_do_end();
	}
	json_do_end();

	json_do_array("bgpsec_keys");
	RB_FOREACH(b, brk_tree, brks) {
		json_do_object("brks");
		json_do_int("asn", b->asid);
		json_do_printf("ski", "%s", b->ski);
		json_do_printf("pubkey", "%s", b->pubkey);
		json_do_printf("ta", "%s", taldescs[b->talid]);
		json_do_int("expires", b->expires);
		json_do_end();
	}
	json_do_end();

	if (!excludeaspa)
		output_aspa(vaps);

	return json_do_finish();
}
