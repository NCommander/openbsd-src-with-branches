/*
 *	$Id: md-static-funcs.c,v 1.2 1993/12/08 10:14:44 pk Exp $
 *
 * Called by ld.so when onanating.
 * This *must* be a static function, so it is not called through a jmpslot.
 */

static void
md_relocate_simple(r, relocation, addr)
struct relocation_info	*r;
long			relocation;
char			*addr;
{
if (r->r_relative)
	*(long *)addr += relocation;
}

