/*	$OpenBSD: comp_parse.c,v 1.2 1998/11/17 03:16:20 millert Exp $	*/

/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/



/*
 *	comp_parse.c -- parser driver loop and use handling.
 *
 *	_nc_read_entry_source(FILE *, literal, bool, bool (*hook)())
 *	_nc_resolve_uses(void)
 *	_nc_free_entries(void)
 *
 *	Use this code by calling _nc_read_entry_source() on as many source
 *	files as you like (either terminfo or termcap syntax).  If you
 *	want use-resolution, call _nc_resolve_uses().  To free the list
 *	storage, do _nc_free_entries().
 *
 */

#include <curses.priv.h>

#include <ctype.h>

#include <tic.h>
#include <term.h>
#include <term_entry.h>

MODULE_ID("$From: comp_parse.c,v 1.23 1998/05/30 23:38:15 Todd.Miller Exp $")

static void sanity_check(TERMTYPE *);

/****************************************************************************
 *
 * Entry queue handling
 *
 ****************************************************************************/
/*
 *  The entry list is a doubly linked list with NULLs terminating the lists:
 *
 *	  ---------   ---------   ---------
 *	  |       |   |       |   |       |   offset
 *        |-------|   |-------|   |-------|
 *	  |   ----+-->|   ----+-->|  NULL |   next
 *	  |-------|   |-------|   |-------|
 *	  |  NULL |<--+----   |<--+----   |   last
 *	  ---------   ---------   ---------
 *	      ^                       ^
 *	      |                       |
 *	      |                       |
 *	   _nc_head                _nc_tail
 */

ENTRY *_nc_head, *_nc_tail;

static void enqueue(ENTRY *ep)
/* add an entry to the in-core list */
{
	ENTRY	*newp = (ENTRY *)malloc(sizeof(ENTRY));

	if (newp == NULL)
	    _nc_err_abort("Out of memory");

	(void) memcpy(newp, ep, sizeof(ENTRY));

	newp->last = _nc_tail;
	_nc_tail = newp;

	newp->next = (ENTRY *)NULL;
	if (newp->last)
	    newp->last->next = newp;
}

void _nc_free_entries(ENTRY *head)
/* free the allocated storage consumed by list entries */
{
    ENTRY	*ep, *next;

    for (ep = head; ep; ep = next)
    {
	/*
	 * This conditional lets us disconnect storage from the list.
	 * To do this, copy an entry out of the list, then null out
	 * the string-table member in the original and any use entries
	 * it references.
	 */
	FreeIfNeeded(ep->tterm.str_table);

	next = ep->next;

	free(ep);
	if (ep == _nc_head) _nc_head = 0;
	if (ep == _nc_tail) _nc_tail = 0;
    }
}

bool _nc_entry_match(char *n1, char *n2)
/* do any of the aliases in a pair of terminal names match? */
{
    char	*pstart, *qstart, *pend, *qend;
    char	nc1[MAX_NAME_SIZE+1], nc2[MAX_NAME_SIZE+1];
    size_t	n;

    if (strchr(n1, '|') == NULL)
    {
	if ((n = strlcpy(nc1, n1, sizeof(nc1))) > sizeof(nc1) - 2)
	    n = sizeof(nc1) - 2;
	nc1[n++] = '|';
	nc1[n] = '\0';
	n1 = nc1;
    }

    if (strchr(n2, '|') == NULL)
    {
	if ((n = strlcpy(nc2, n2, sizeof(nc2))) > sizeof(nc2) - 2)
	    n = sizeof(nc2) - 2;
	nc2[n++] = '|';
	nc2[n] = '\0';
	n2 = nc2;
    }

    for (pstart = n1; (pend = strchr(pstart, '|')); pstart = pend + 1)
	for (qstart = n2; (qend = strchr(qstart, '|')); qstart = qend + 1)
	    if ((pend-pstart == qend-qstart)
	     && memcmp(pstart, qstart, (size_t)(pend-pstart)) == 0)
		return(TRUE);

	return(FALSE);
}

/****************************************************************************
 *
 * Entry compiler and resolution logic
 *
 ****************************************************************************/

void _nc_read_entry_source(FILE *fp, char *buf,
			   int literal, bool silent,
			   bool (*hook)(ENTRY *))
/* slurp all entries in the given file into core */
{
    ENTRY	thisentry;
    bool	oldsuppress = _nc_suppress_warnings;
    int		immediate = 0;

    if (silent)
	_nc_suppress_warnings = TRUE;	/* shut the lexer up, too */

    for (_nc_reset_input(fp, buf); _nc_parse_entry(&thisentry, literal, silent) != ERR; )
    {
	if (!isalnum(thisentry.tterm.term_names[0]))
	    _nc_err_abort("terminal names must start with letter or digit");

	/*
	 * This can be used for immediate compilation of entries with no
	 * use references to disk, so as to avoid chewing up a lot of
	 * core when the resolution code could fetch entries off disk.
	 */
	if (hook != NULLHOOK && (*hook)(&thisentry))
	    immediate++;
	else
	    enqueue(&thisentry);
    }

    if (_nc_tail)
    {
	/* set up the head pointer */
	for (_nc_head = _nc_tail; _nc_head->last; _nc_head = _nc_head->last)
	    continue;

	DEBUG(1, ("head = %s", _nc_head->tterm.term_names));
	DEBUG(1, ("tail = %s", _nc_tail->tterm.term_names));
    }
#ifdef TRACE
    else if (!immediate)
	DEBUG(1, ("no entries parsed"));
#endif

    _nc_suppress_warnings = oldsuppress;
}

int _nc_resolve_uses(void)
/* try to resolve all use capabilities */
{
    ENTRY	*qp, *rp, *lastread = NULL;
    bool	keepgoing;
    int		i, j, unresolved, total_unresolved, multiples;

    DEBUG(2, ("RESOLUTION BEGINNING"));

    /*
     * Check for multiple occurrences of the same name.
     */
    multiples = 0;
    for_entry_list(qp)
    {
	int matchcount = 0;

	for_entry_list(rp)
	    if (qp > rp
		&& _nc_entry_match(qp->tterm.term_names, rp->tterm.term_names))
	    {
		matchcount++;
		if (matchcount == 1)
		{
		    (void) fprintf(stderr, "Name collision between %s",
			   _nc_first_name(qp->tterm.term_names));
		    multiples++;
		}
		if (matchcount >= 1)
		    (void) fprintf(stderr, " %s", _nc_first_name(rp->tterm.term_names));
	    }
	if (matchcount >= 1)
	    (void) putc('\n', stderr);
    }
    if (multiples > 0)
	return(FALSE);

    DEBUG(2, ("NO MULTIPLE NAME OCCURRENCES"));

    /*
     * First resolution stage: replace names in use arrays with entry
     * pointers.  By doing this, we avoid having to do the same name
     * match once for each time a use entry is itself unresolved.
     */
    total_unresolved = 0;
    _nc_curr_col = -1;
    for_entry_list(qp)
    {
	unresolved = 0;
	for (i = 0; i < qp->nuses; i++)
	{
	    bool	foundit;
	    char	*child = _nc_first_name(qp->tterm.term_names);
	    char	*lookfor = (char *)(qp->uses[i].parent);
	    long	lookline = qp->uses[i].line;

	    foundit = FALSE;

	    _nc_set_type(child);

	    /* first, try to resolve from in-core records */
	    for_entry_list(rp)
		if (rp != qp
		    && _nc_name_match(rp->tterm.term_names, lookfor, "|"))
		{
		    DEBUG(2, ("%s: resolving use=%s (in core)",
			      child, lookfor));

		    qp->uses[i].parent = rp;
		    foundit = TRUE;
		}

	    /* if that didn't work, try to merge in a compiled entry */
	    if (!foundit)
	    {
		TERMTYPE	thisterm;
		char		filename[PATH_MAX];

		if (_nc_read_entry(lookfor, filename, &thisterm) == 1)
		{
		    DEBUG(2, ("%s: resolving use=%s (compiled)",
			      child, lookfor));

		    rp = (ENTRY *)malloc(sizeof(ENTRY));
		    if (rp == NULL)
			_nc_err_abort("Out of memory");
		    memcpy(&rp->tterm, &thisterm, sizeof(TERMTYPE));
		    rp->nuses = 0;
		    rp->next = lastread;
		    lastread = rp;

		    qp->uses[i].parent = rp;
		    foundit = TRUE;
		}
	    }

	    /* no good, mark this one unresolvable and complain */
	    if (!foundit)
	    {
		unresolved++;
		total_unresolved++;

		_nc_curr_line = lookline;
		_nc_warning("resolution of use=%s failed", lookfor);
		qp->uses[i].parent = (ENTRY *)NULL;
	    }
	}
    }
    if (total_unresolved)
    {
	/* free entries read in off disk */
	_nc_free_entries(lastread);
	return(FALSE);
    }

    DEBUG(2, ("NAME RESOLUTION COMPLETED OK"));

    /*
     * OK, at this point all (char *) references have been successfully
     * replaced by (ENTRY *) pointers.  Time to do the actual merges.
     */
    do {
	TERMTYPE	merged;

	keepgoing = FALSE;

	for_entry_list(qp)
	    if (qp->nuses > 0)
	    {
		DEBUG(2, ("%s: attempting merge", _nc_first_name(qp->tterm.term_names)));
		/*
		 * If any of the use entries we're looking for is
		 * incomplete, punt.  We'll catch this entry on a
		 * subsequent pass.
		 */
		for (i = 0; i < qp->nuses; i++)
		    if (((ENTRY *)qp->uses[i].parent)->nuses)
		    {
			DEBUG(2, ("%s: use entry %d unresolved",
				  _nc_first_name(qp->tterm.term_names), i));
			goto incomplete;
		    }

		/*
		 * First, make sure there's no garbage in the merge block.
		 * as a side effect, copy into the merged entry the name
		 * field and string table pointer.
		 */
		memcpy(&merged, &qp->tterm, sizeof(TERMTYPE));

		/*
		 * Now merge in each use entry in the proper
		 * (reverse) order.
		 */
		for (; qp->nuses; qp->nuses--)
		    _nc_merge_entry(&merged,
				&((ENTRY *)qp->uses[qp->nuses-1].parent)->tterm);

		/*
		 * Now merge in the original entry.
		 */
		_nc_merge_entry(&merged, &qp->tterm);

		/*
		 * Replace the original entry with the merged one.
		 */
		memcpy(&qp->tterm, &merged, sizeof(TERMTYPE));

		/*
		 * We know every entry is resolvable because name resolution
		 * didn't bomb.  So go back for another pass.
		 */
		/* FALLTHRU */
	    incomplete:
		keepgoing = TRUE;
	    }
    } while
	(keepgoing);

    DEBUG(2, ("MERGES COMPLETED OK"));

    /*
     * The exit condition of the loop above is such that all entries
     * must now be resolved.  Now handle cancellations.  In a resolved
     * entry there should be no cancellation markers.
     */
    for_entry_list(qp)
    {
	for (j = 0; j < BOOLCOUNT; j++)
	    if (qp->tterm.Booleans[j] == CANCELLED_BOOLEAN)
		qp->tterm.Booleans[j] = FALSE;
	for (j = 0; j < NUMCOUNT; j++)
	    if (qp->tterm.Numbers[j] == CANCELLED_NUMERIC)
		qp->tterm.Numbers[j] = ABSENT_NUMERIC;
	for (j = 0; j < STRCOUNT; j++)
	    if (qp->tterm.Strings[j] == CANCELLED_STRING)
		qp->tterm.Strings[j] = ABSENT_STRING;
    }

    /*
     * We'd like to free entries read in off disk at this point, but can't.
     * The merge_entry() code doesn't copy the strings in the use entries,
     * it just aliases them.  If this ever changes, do a
     * free_entries(lastread) here.
     */

    DEBUG(2, ("RESOLUTION FINISHED"));

    _nc_curr_col = -1;
    for_entry_list(qp)
    {
	_nc_curr_line = qp->startline;
	_nc_set_type(_nc_first_name(qp->tterm.term_names));
	sanity_check(&qp->tterm);
    }

    DEBUG(2, ("SANITY CHECK FINISHED"));

    return(TRUE);
}

/*
 * This bit of legerdemain turns all the terminfo variable names into
 * references to locations in the arrays Booleans, Numbers, and Strings ---
 * precisely what's needed.
 */

#undef CUR
#define CUR tp->

/*
 * Note that WANTED and PRESENT are not simple inverses!  If a capability
 * has been explicitly cancelled, it's not considered WANTED.
 */
#define WANTED(s)	((s) == ABSENT_STRING)
#define PRESENT(s)	(((s) != ABSENT_STRING) && ((s) != CANCELLED_STRING))

#define ANDMISSING(p,q) \
		{if (PRESENT(p) && !PRESENT(q)) _nc_warning(#p " but no " #q);}

#define PAIRED(p,q) \
		{ \
		if (PRESENT(q) && !PRESENT(p)) \
			_nc_warning(#q " but no " #p); \
		if (PRESENT(p) && !PRESENT(q)) \
			_nc_warning(#p " but no " #q); \
		}

static void sanity_check(TERMTYPE *tp)
{
#ifdef __UNUSED__	/* this casts too wide a net */
    bool       terminal_entry = !strchr(tp->term_names, '+');
#endif

    if (!PRESENT(exit_attribute_mode))
    {
#ifdef __UNUSED__	/* this casts too wide a net */
	if (terminal_entry &&
		(PRESENT(set_attributes)
		|| PRESENT(enter_standout_mode)
		|| PRESENT(enter_underline_mode)
		|| PRESENT(enter_blink_mode)
		|| PRESENT(enter_bold_mode)
		|| PRESENT(enter_dim_mode)
		|| PRESENT(enter_secure_mode)
		|| PRESENT(enter_protected_mode)
		|| PRESENT(enter_reverse_mode)))
	    _nc_warning("no exit_attribute_mode");
#endif /* __UNUSED__ */
	PAIRED(enter_standout_mode,     exit_standout_mode)
	PAIRED(enter_underline_mode,    exit_underline_mode)
    }

     /* listed in structure-member order of first argument */
#ifdef __UNUSED__
     ANDMISSING(cursor_invisible,            cursor_normal)
     ANDMISSING(cursor_visible,              cursor_normal)
#endif /* __UNUSED__ */
     PAIRED(enter_alt_charset_mode,          exit_alt_charset_mode)
     ANDMISSING(enter_alt_charset_mode,      acs_chars)
     ANDMISSING(exit_alt_charset_mode,       acs_chars)
     ANDMISSING(enter_blink_mode,            exit_attribute_mode)
     ANDMISSING(enter_bold_mode,             exit_attribute_mode)
     PAIRED(exit_ca_mode,                    enter_ca_mode)
     PAIRED(enter_delete_mode,               exit_delete_mode)
     ANDMISSING(enter_dim_mode,              exit_attribute_mode)
     PAIRED(enter_insert_mode,               exit_insert_mode)
     ANDMISSING(enter_secure_mode,           exit_attribute_mode)
     ANDMISSING(enter_protected_mode,        exit_attribute_mode)
     ANDMISSING(enter_reverse_mode,          exit_attribute_mode)
     PAIRED(from_status_line,                to_status_line)
     PAIRED(meta_off,                        meta_on)

     PAIRED(prtr_on,                         prtr_off)
     PAIRED(save_cursor,                     restore_cursor)
     PAIRED(enter_xon_mode,                  exit_xon_mode)
     PAIRED(enter_am_mode,                   exit_am_mode)
     ANDMISSING(label_off,                   label_on)
     PAIRED(display_clock,                   remove_clock)
     ANDMISSING(set_color_pair,              initialize_pair)

     /* Some checks that we should make, but don't want to confuse people
      * with.  Put those under the tic -v option so we can still get them.
      */
     if (_nc_tracing) {

	/*
	 * From XSI & O'Reilly, we gather that sc/rc are required if csr is
	 * given, because the cursor position after the scrolling operation is
	 * performed is undefined.
	 */
         ANDMISSING(change_scroll_region,        save_cursor)
         ANDMISSING(change_scroll_region,        restore_cursor)

         /*
	  * Some non-curses applications (e.g., jove) get confused if we have
	  * both ich/ich1 and smir/rmir.  Let's be nice and warn about that,
	  * too, even though ncurses handles it.
          */
         if ((PRESENT(enter_insert_mode) || PRESENT(exit_insert_mode))
          && (PRESENT(insert_character)  || PRESENT(parm_ich))) {
	    _nc_warning("non-curses applications may be confused by ich/ich1 with smir/rmir");
         }
     }
#undef PAIRED
#undef ANDMISSING
}
