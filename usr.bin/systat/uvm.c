/*	$OpenBSD$	*/
/*
 * Copyright (c) 2008 Can Erkin Acar <canacar@openbsd.org>
 * Copyright (c) 2018 Kenneth R Westerback <krw@openbsd.org>
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
#include <sys/signal.h>
#include <sys/sysctl.h>
#include <sys/pool.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "systat.h"

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

void print_uvm(void);
int  read_uvm(void);
int  select_uvm(void);

void print_uvmexp_field(field_def *, field_def *, int *, int *, const char *);
void print_uvmexp_line(int);

struct uvmexp uvmexp;
struct uvmexp last_uvmexp;

struct uvmline {
	int	*v1;
	int	*ov1;
	char	*n1;
	int	*v2;
	int	*ov2;
	char	*n2;
	int	*v3;
	int	*ov3;
	char	*n3;
};

struct uvmline uvmline[] = {
	{ NULL, NULL, "Page Counters",
	  NULL, NULL, "Stats Counters",
	  NULL, NULL, "Fault Counters" },
	{ &uvmexp.npages, &last_uvmexp.npages, "npages",
	  &uvmexp.faults, &last_uvmexp.faults, "faults",
	  &uvmexp.fltnoram, &last_uvmexp.fltnoram, "fltnoram" },
	{ &uvmexp.free, &last_uvmexp.free, "free",
	  &uvmexp.traps, &last_uvmexp.traps, "traps",
	  &uvmexp.fltnoanon, &last_uvmexp.fltnoanon, "fltnoanon" },
	{ &uvmexp.active, &last_uvmexp.active, "active",
	  &uvmexp.intrs, &last_uvmexp.intrs, "intrs",
	  &uvmexp.fltnoamap, &last_uvmexp.fltnoamap, "fltnoamap" },
	{ &uvmexp.inactive, &last_uvmexp.inactive, "inactive",
	  &uvmexp.swtch, &last_uvmexp.swtch, "swtch",
	  &uvmexp.fltpgwait, &last_uvmexp.fltpgwait, "fltpgwait" },
	{ &uvmexp.paging, &last_uvmexp.paging, "paging",
	  &uvmexp.softs, &last_uvmexp.softs, "softs",
	  &uvmexp.fltpgrele, &last_uvmexp.fltpgrele, "fltpgrele" },
	{ &uvmexp.wired, &last_uvmexp.wired, "wired",
	  &uvmexp.syscalls, &last_uvmexp.syscalls, "syscalls",
	  &uvmexp.fltrelck, &last_uvmexp.fltrelck, "fltrelck" },
	{ &uvmexp.zeropages, &last_uvmexp.zeropages, "zeropages",
	  &uvmexp.pageins, &last_uvmexp.pageins, "pageins",
	  &uvmexp.fltrelckok, &last_uvmexp.fltrelckok, "fltrelckok" },
	{ &uvmexp.reserve_pagedaemon, &last_uvmexp.reserve_pagedaemon,
	  "reserve_pagedaemon",
	  &uvmexp.obsolete_swapins, &last_uvmexp.obsolete_swapins,
	  "obsolete_swapins",
	  &uvmexp.fltanget, &last_uvmexp.fltanget, "fltanget" },
	{ &uvmexp.reserve_kernel, &last_uvmexp.reserve_kernel, "reserve_kernel",
	  &uvmexp.obsolete_swapouts, &last_uvmexp.obsolete_swapouts,
	  "obsolete_swapouts",
	  &uvmexp.fltanretry, &last_uvmexp.fltanretry, "fltanretry" },
	{ &uvmexp.anonpages, &last_uvmexp.anonpages, "anonpages",
	  &uvmexp.pgswapin, &last_uvmexp.pgswapin, "pgswapin",
	  &uvmexp.fltamcopy, &last_uvmexp.fltamcopy, "fltamcopy" },
	{ &uvmexp.vnodepages, &last_uvmexp.vnodepages, "vnodepages",
	  &uvmexp.pgswapout, &last_uvmexp.pgswapout, "pgswapout",
	  &uvmexp.fltnamap, &last_uvmexp.fltnamap, "fltnamap" },
	{ &uvmexp.vtextpages, &last_uvmexp.vtextpages, "vtextpages",
	  &uvmexp.forks, &last_uvmexp.forks, "forks",
	  &uvmexp.fltnomap, &last_uvmexp.fltnomap, "fltnomap" },
	{ NULL, NULL, NULL,
	  &uvmexp.forks_ppwait, &last_uvmexp.forks_ppwait, "forks_ppwait",
	  &uvmexp.fltlget, &last_uvmexp.fltlget, "fltlget" },
	{ NULL, NULL, "Pageout Params",
	  &uvmexp.forks_sharevm, &last_uvmexp.forks_sharevm, "forks_sharevm",
	  &uvmexp.fltget, &last_uvmexp.fltget, "fltget" },
	{ &uvmexp.freemin, &last_uvmexp.freemin, "freemin",
	  &uvmexp.pga_zerohit, &last_uvmexp.pga_zerohit, "pga_zerohit",
	  &uvmexp.flt_anon, &last_uvmexp.flt_anon, "flt_anon" },
	{ &uvmexp.freetarg, &last_uvmexp.freetarg, "freetarg",
	  &uvmexp.pga_zeromiss, &last_uvmexp.pga_zeromiss, "pga_zeromiss",
	  &uvmexp.flt_acow, &last_uvmexp.flt_acow, "flt_acow" },
	{ &uvmexp.inactarg, &last_uvmexp.inactarg, "inactarg",
	  &uvmexp.zeroaborts, &last_uvmexp.zeroaborts, "zeroaborts",
	  &uvmexp.flt_obj, &last_uvmexp.flt_obj, "flt_obj" },
	{ &uvmexp.wiredmax, &last_uvmexp.wiredmax, "wiredmax",
	  NULL, NULL, NULL,
	  &uvmexp.flt_prcopy, &last_uvmexp.flt_prcopy, "flt_prcopy" },
	{ &uvmexp.anonmin, &last_uvmexp.anonmin, "anonmin",
	  NULL, NULL, "Swap Counters",
	  &uvmexp.flt_przero, &last_uvmexp.flt_przero, "flt_przero" },
	{ &uvmexp.vtextmin, &last_uvmexp.vtextmin, "vtextmin",
	  &uvmexp.nswapdev, &last_uvmexp.nswapdev, "nswapdev",
	  NULL, NULL, NULL },
	{ &uvmexp.vnodemin, &last_uvmexp.vnodemin, "vnodemin",
	  &uvmexp.swpages, &last_uvmexp.swpages, "swpages",
	  NULL, NULL, "Daemon Counters" },
	{ &uvmexp.anonminpct, &last_uvmexp.anonminpct, "anonminpct",
	  &uvmexp.swpginuse, &last_uvmexp.swpginuse, "swpginuse",
	  &uvmexp.pdwoke, &last_uvmexp.pdwoke, "pdwoke" },
	{ &uvmexp.vtextminpct, &last_uvmexp.vtextminpct, "vtextminpct",
	  &uvmexp.swpgonly, &last_uvmexp.swpgonly, "swpgonly",
	  &uvmexp.pdrevs, &last_uvmexp.pdrevs, "pdrevs" },
	{ &uvmexp.vnodeminpct, &last_uvmexp.vnodeminpct, "vnodeminpct",
	  &uvmexp.nswget, &last_uvmexp.nswget, "nswget",
	  &uvmexp.pdswout, &last_uvmexp.pdswout, "pdswout" },
	{ NULL, NULL, NULL,
	  &uvmexp.nanon, &last_uvmexp.nanon, "nanon",
	  &uvmexp.pdfreed, &last_uvmexp.pdfreed, "pdfreed" },
	{ NULL, NULL, "Misc Counters",
	  &uvmexp.nanonneeded, &last_uvmexp.nanonneeded, "nanonneeded",
	  &uvmexp.pdscans, &last_uvmexp.pdscans, "pdscans" },
	{ &uvmexp.fpswtch, &last_uvmexp.fpswtch, "fpswtch",
	  &uvmexp.nfreeanon, &last_uvmexp.nfreeanon, "nfreeanon",
	  &uvmexp.pdanscan, &last_uvmexp.pdanscan, "pdanscan" },
	{ &uvmexp.kmapent, &last_uvmexp.kmapent, "kmapent",
	  NULL, NULL, NULL,
	  &uvmexp.pdobscan, &last_uvmexp.pdobscan, "pdobscan" },
	{ NULL, NULL, NULL,
	  NULL, NULL, NULL,
	  &uvmexp.pdreact, &last_uvmexp.pdreact, "pdreact" },
	{ NULL, NULL, "Constants",
	  NULL, NULL, NULL,
	  &uvmexp.pdbusy, &last_uvmexp.pdbusy, "pdbusy" },
	{ &uvmexp.pagesize, &last_uvmexp.pagesize, "pagesize",
	  NULL, NULL, NULL,
	  &uvmexp.pdpageouts, &last_uvmexp.pdpageouts, "pdpageouts" },
	{ &uvmexp.pagemask, &last_uvmexp.pagemask, "pagemask",
	  NULL, NULL, NULL,
	  &uvmexp.pdpending, &last_uvmexp.pdpending, "pdpending" },
	{ &uvmexp.pageshift, &last_uvmexp.pageshift, "pageshift",
	  NULL, NULL, NULL,
	  &uvmexp.pddeact, &last_uvmexp.pddeact, "pddeact" },
	{ NULL, NULL, NULL,
	  NULL, NULL, NULL,
	  &uvmexp.pdreanon, &last_uvmexp.pdreanon, "pdrenon" },
	{ NULL, NULL, NULL,
	  NULL, NULL, NULL,
	  &uvmexp.pdrevnode, &last_uvmexp.pdrevnode, "pdrevnode" },
	{ NULL, NULL, NULL,
	  NULL, NULL, NULL,
	  &uvmexp.pdrevtext, &last_uvmexp.pdrevtext, "pdrevtext" }
};

field_def fields_uvm[] = {
	{"",	 5,10,1, FLD_ALIGN_RIGHT,	-1,0,0,0 },
	{"",	18,19,1, FLD_ALIGN_LEFT,	-1,0,0,0 },
	{"",	 5,10,1, FLD_ALIGN_RIGHT,	-1,0,0,0 },
	{"",	18,19,1, FLD_ALIGN_LEFT,	-1,0,0,0 },
	{"",	 5,10,1, FLD_ALIGN_RIGHT,	-1,0,0,0 },
	{"",	18,19,1, FLD_ALIGN_LEFT,	-1,0,0,0 },
};

#define	FLD_VALUE1		FIELD_ADDR(fields_uvm,  0)
#define	FLD_NAME1		FIELD_ADDR(fields_uvm,  1)
#define	FLD_VALUE2		FIELD_ADDR(fields_uvm,  2)
#define	FLD_NAME2		FIELD_ADDR(fields_uvm,  3)
#define	FLD_VALUE3		FIELD_ADDR(fields_uvm,  4)
#define	FLD_NAME3		FIELD_ADDR(fields_uvm,  5)

/* Define views */
field_def *view_uvm_0[] = {
	FLD_VALUE1, FLD_NAME1,
	FLD_VALUE2, FLD_NAME2,
	FLD_VALUE3, FLD_NAME3,
	NULL
};

/* Define view managers */
struct view_manager uvm_mgr = {
	"UVM", select_uvm, read_uvm, NULL, print_header,
	print_uvm, keyboard_callback, NULL, NULL
};

field_view uvm_view = {
	view_uvm_0,
	"uvm",
	'5',
	&uvm_mgr
};

int
select_uvm(void)
{
	return (0);
}

int
read_uvm(void)
{
	static int uvmexp_mib[2] = { CTL_VM, VM_UVMEXP };
	size_t size;

	num_disp = nitems(uvmline);
	memcpy(&last_uvmexp, &uvmexp, sizeof(uvmexp));

	size = sizeof(uvmexp);
	if (sysctl(uvmexp_mib, 2, &uvmexp, &size, NULL, 0) < 0) {
		error("Can't get VM_UVMEXP: %s\n", strerror(errno));
		memset(&uvmexp, 0, sizeof(uvmexp));
	}

	return 0;
}

void
print_uvmexp_field(field_def *fvalue, field_def *fname, int *new, int *old,
    const char *name)
{
	char *uppername;
	size_t i;

	if (new == NULL && name == NULL)
		return;

	if (new == NULL) {
		print_fld_str(fvalue, "=====");
		print_fld_str(fname, name);
		return;
	}

	if (*new != 0)
		print_fld_ssize(fvalue, *new);
	if (*new == *old) {
		print_fld_str(fname, name);
		return;
	}
	uppername = calloc(1, strlen(name) + 1);
	if (uppername == NULL)
		err(1, "calloc");
	for (i = 0; i < strlen(name); i++)
		uppername[i] = toupper(name[i]);
	print_fld_str(fname, uppername);
	free(uppername);
}

void
print_uvm(void)
{
	struct uvmline *l;
	int i, maxline;

	maxline = nitems(uvmline);
	if (maxline > (dispstart + maxprint))
		maxline = dispstart + maxprint;

	for (i = dispstart; i < nitems(uvmline); i++) {
		l = &uvmline[i];
		print_uvmexp_field(FLD_VALUE1, FLD_NAME1, l->v1, l->ov1, l->n1);
		print_uvmexp_field(FLD_VALUE2, FLD_NAME2, l->v2, l->ov2, l->n2);
		print_uvmexp_field(FLD_VALUE3, FLD_NAME3, l->v3, l->ov3, l->n3);
		end_line();
	}
}

int
inituvm(void)
{
	add_view(&uvm_view);
	read_uvm();

	return(0);
}
