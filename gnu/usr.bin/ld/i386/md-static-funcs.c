/* *	$OpenBSD: md-static-funcs.c,v 1.3 2002/07/15 21:05:57 marc Exp $*/
/*
 *
 * Called by ld.so when onanating.
 * This *must* be a static function, so it is not called through a jmpslot.
 */

static void
md_relocate_simple(struct relocation_info *r, long relocation, char *addr)
{
	if (r->r_relative)
		*(long *)addr += relocation;
}
