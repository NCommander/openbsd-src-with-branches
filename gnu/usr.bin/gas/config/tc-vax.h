/*	$OpenBSD: tc-vax.h,v 1.4 2001/11/14 16:21:02 hugh Exp $	*/

/*
 * This file is tc-vax.h.
 */

#define TC_VAX 1

#define AOUT_MACHTYPE 150

#define	LOCAL_LABELS_FB

 /* use this to compare against gas-1.38 */
#ifdef OLD_GAS
#define REVERSE_SORT_RELOCS
#endif

#define tc_aout_pre_write_hook(x)	{;} /* not used */
#define tc_crawl_symbol_chain(a)	{;} /* not used */
#define tc_headers_hook(a)		{;} /* not used */

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of tc-vax.h */
