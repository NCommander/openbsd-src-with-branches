/*	$OpenBSD$	*/


/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/

/******************************************************************************

NAME
   hashmap.c -- fill in scramble vector based on text hashes

SYNOPSIS
   void _nc_hash_map(void)

DESCRIPTION:
   This code attempts to recognize pairs of old and new lines in the physical
and virtual screens.  When a line pair is recognized, the old line index is
placed in the oldindex member of the virtual screen line, to be used by the
vertical-motion optimizer portion of the update logic (see hardscroll.c).

   Line pairs are recognized by applying a modified Heckel's algorithm,
sped up by hashing.  If a line hash is unique in both screens, those
lines must be a pair. Then if the lines just before or after the pair
are the same or similar, they are a pair too.

   We don't worry about false pairs produced by hash collisions, on the
assumption that such cases are rare and will only make the latter stages
of update less efficient, not introduce errors.

HOW TO TEST THIS:

Use the following production:

hashmap: hashmap.c
	$(CC) -g -DHASHDEBUG hashmap.c hardscroll.c ../objects/lib_trace.o -o hashmap

AUTHOR
    Eric S. Raymond <esr@snark.thyrsus.com>, May 1996

*****************************************************************************/

#include <curses.priv.h>

MODULE_ID("Id: hashmap.c,v 1.23 1997/10/18 17:25:02 tom Exp $")

#ifdef HASHDEBUG
#define TEXTWIDTH	1
int oldnums[MAXLINES], reallines[MAXLINES];
static chtype oldtext[MAXLINES][TEXTWIDTH], newtext[MAXLINES][TEXTWIDTH];
#define OLDNUM(n)	oldnums[n]
#define REAL(m)		reallines[m]
#define OLDTEXT(n)	oldtext[n]
#define NEWTEXT(m)	newtext[m]
#undef T
#define T(x)		(void) printf x ; (void) putchar('\n');
#else
#include <curses.h>
#define OLDNUM(n)	newscr->_line[n].oldindex
#define REAL(m)		curscr->_line[m].oldindex
#define OLDTEXT(n)	curscr->_line[n].text
#define NEWTEXT(m)	newscr->_line[m].text
#define TEXTWIDTH	(curscr->_maxx+1)
#ifndef _NEWINDEX
#define _NEWINDEX	-1
#endif /* _NEWINDEX */
#endif /* HASHDEBUG */

static inline unsigned long hash(chtype *text)
{
    int i;
    chtype ch;
    unsigned long result = 0;
    for (i = TEXTWIDTH; i>0; i--)
    {
	ch = *text++;
	result += (result<<5) + ch + (ch>>16);
    }
    return result;
}

/* approximate update cost */
static int update_cost(chtype *from,chtype *to)
{
    int cost=0;
    int i;

    for (i=TEXTWIDTH; i>0; i--)
        if (*from++ != *to++)
	    cost++;

    return cost;
}
static int update_cost_from_blank(chtype *to)
{
    int cost=0;
    int i;

    /* FIXME: ClrBlank should be used */
    for (i=TEXTWIDTH; i>0; i--)
        if (BLANK != *to++)
	    cost++;

    return cost;
}

/*
 * Returns true when moving line 'from' to line 'to' seems to be cost
 * effective. 'blank' indicates whether the line 'to' would become blank.
 */
static inline bool cost_effective(const int from, const int to, const bool blank)
{
    int new_from;

    if (from == to)
	return FALSE;

    new_from = OLDNUM(from);
    if (new_from == _NEWINDEX)
	new_from = from;

    /* 
     * On the left side of >= is the cost before moving;
     * on the right side -- cost after moving.
     */
    return (((blank ? update_cost_from_blank(NEWTEXT(to))
		    : update_cost(OLDTEXT(to),NEWTEXT(to)))
	     + update_cost(OLDTEXT(new_from),NEWTEXT(from)))
	 >= ((new_from==from ? update_cost_from_blank(NEWTEXT(from))
			     : update_cost(OLDTEXT(new_from),NEWTEXT(from)))
	     + update_cost(OLDTEXT(from),NEWTEXT(to)))) ? TRUE : FALSE;
}


typedef struct
{
    unsigned long	hashval;
    int		oldcount, newcount;
    int		oldindex, newindex;
}
    sym;

static sym *hashtab=0;
static int lines_alloc=0; 
static long *oldhash=0;
static long *newhash=0;

static void grow_hunks(void)
{
    int start, end, shift;
    int back_limit, forward_limit;	    /* limits for cells to fill */
    int back_ref_limit, forward_ref_limit;  /* limits for refrences */
    int i;
    int next_hunk;

    /*
     * This is tricky part.  We have unique pairs to use as anchors.
     * Use these to deduce the presence of spans of identical lines.
     */
    back_limit = 0;
    back_ref_limit = 0;

    i = 0;
    while (i < screen_lines && OLDNUM(i) == _NEWINDEX)
	i++;
    for ( ; i < screen_lines; i=next_hunk)
    {
	start = i;
	shift = OLDNUM(i) - i;
	
	/* get forward limit */
	i = start+1;
	while (i < screen_lines && OLDNUM(i) != _NEWINDEX && OLDNUM(i) - i == shift)
	    i++;
	end = i;
	while (i < screen_lines && OLDNUM(i) == _NEWINDEX)
	    i++;
	next_hunk = i;
	forward_limit = i;
	if (i >= screen_lines || OLDNUM(i) >= i)
	    forward_ref_limit = i;
	else
	    forward_ref_limit = OLDNUM(i);

	i = start-1;
	/* grow back */
	if (shift < 0)
	    back_limit = back_ref_limit + (-shift);
	while (i >= back_limit)
	{
	    if(newhash[i] == oldhash[i+shift]
	    || cost_effective(i+shift, i, shift<0))
	    {
		OLDNUM(i) = i+shift;
		TR(TRACE_UPDATE | TRACE_MOVE,
		   ("connected new line %d to old line %d (backward continuation)",
		    i, i+shift));
	    }
	    else
	    {
		TR(TRACE_UPDATE | TRACE_MOVE,
		   ("not connecting new line %d to old line %d (backward continuation)",
		    i, i+shift));
		break;
	    }
	    i--;
	}
	
	i = end;
	/* grow forward */
	if (shift > 0)
	    forward_limit = forward_ref_limit - shift;
	while (i < forward_limit)
	{
	    if(newhash[i] == oldhash[i+shift]
	    || cost_effective(i+shift, i, shift>0))
	    {
		OLDNUM(i) = i+shift;
		TR(TRACE_UPDATE | TRACE_MOVE,
		   ("connected new line %d to old line %d (forward continuation)",
		    i, i+shift));
	    }
	    else
	    {
		TR(TRACE_UPDATE | TRACE_MOVE,
		   ("not connecting new line %d to old line %d (forward continuation)",
		    i, i+shift));
		break;
	    }
	    i++;
	}
	
	back_ref_limit = back_limit = i;
	if (shift > 0)
	    back_ref_limit += shift;
    }
}

void _nc_hash_map(void)
{
    sym *sp;
    register int i;
    int start, shift, size;


    if (screen_lines > lines_alloc)
    {
	if (hashtab)
	    free (hashtab);
	hashtab = malloc (sizeof(*hashtab)*(screen_lines+1)*2);
	if (!hashtab)
	{
	    if (oldhash)
		FreeAndNull(oldhash);
	    lines_alloc = 0;
	    return;
	}
  
	if (oldhash)
	    free (oldhash);
	oldhash = malloc (sizeof(*oldhash)*screen_lines*2);
	if (!oldhash)
	{
	    if (hashtab)
		FreeAndNull(hashtab);
	    lines_alloc = 0;
	    return;
	}
	
	lines_alloc = screen_lines;
    }
    newhash = oldhash + screen_lines;	/* two arrays in the same memory block */

    /*
     * Set up and count line-hash values.
     */
    memset(hashtab, '\0', sizeof(*hashtab)*(screen_lines+1)*2);
    for (i = 0; i < screen_lines; i++)
    {
	unsigned long hashval = hash(OLDTEXT(i));

	for (sp = hashtab; sp->hashval; sp++)
	    if (sp->hashval == hashval)
		break;
	sp->hashval = hashval;	/* in case this is a new entry */
	oldhash[i] = hashval;
	sp->oldcount++;
	sp->oldindex = i;
    }
    for (i = 0; i < screen_lines; i++)
    {
	unsigned long hashval = hash(NEWTEXT(i));

	for (sp = hashtab; sp->hashval; sp++)
	    if (sp->hashval == hashval)
		break;
	sp->hashval = hashval;	/* in case this is a new entry */
	newhash[i] = hashval;
	sp->newcount++;
	sp->newindex = i;
    
	OLDNUM(i) = _NEWINDEX;
    }

    /*
     * Mark line pairs corresponding to unique hash pairs.
     * 
     * We don't mark lines with offset 0, because it can make fail
     * extending hunks by cost_effective. Otherwise, it does not
     * have any side effects.
     */
    for (sp = hashtab; sp->hashval; sp++)
	if (sp->oldcount == 1 && sp->newcount == 1
	    && sp->oldindex != sp->newindex)
	{
	    TR(TRACE_UPDATE | TRACE_MOVE,
	       ("new line %d is hash-identical to old line %d (unique)",
		   sp->newindex, sp->oldindex));
	    OLDNUM(sp->newindex) = sp->oldindex;
	}

    grow_hunks();

    /*
     * Eliminate bad or impossible shifts -- this includes removing
     * those hunks which could not grow because of conflicts, as well
     * those which are to be moved too far, they are likely to destroy
     * more than carry.
     */
    for (i = 0; i < screen_lines; )
    {
	while (i < screen_lines && OLDNUM(i) == _NEWINDEX)
	    i++;
	if (i >= screen_lines)
	    break;
	start = i;
	shift = OLDNUM(i) - i;
	i++;
	while (i < screen_lines && OLDNUM(i) != _NEWINDEX && OLDNUM(i) - i == shift)
	    i++;
	size = i - start;
	if (size <= abs(shift))
	{
	    while (start < i)
	    {
		OLDNUM(start) = _NEWINDEX;
		start++;
	    }
	}
    }
    
    /* After clearing invalid hunks, try grow the rest. */
    grow_hunks();

#if NO_LEAKS
    FreeAndNull(hashtab);
    FreeAndNull(oldhash);
    lines_alloc = 0;
#endif
}

#ifdef HASHDEBUG

int
main(int argc GCC_UNUSED, char *argv[] GCC_UNUSED)
{
    char	line[BUFSIZ], *st;
    int		n;

    for (n = 0; n < screen_lines; n++)
    {
	reallines[n] = n;
	oldnums[n] = _NEWINDEX;
	oldtext[n][0] = newtext[n][0] = '.';
    }

#ifdef TRACE
    _nc_tracing = TRACE_MOVE;
#endif
    for (;;)
    {
	/* grab a test command */
	if (fgets(line, sizeof(line), stdin) == (char *)NULL)
	    exit(EXIT_SUCCESS);

	switch(line[0])
	{
	case '#':	/* comment */
	    (void) fputs(line, stderr);
	    break;

	case 'l':	/* get initial line number vector */
	    for (n = 0; n < screen_lines; n++)
	    {
		reallines[n] = n;
		oldnums[n] = _NEWINDEX;
	    }
	    n = 0;
	    st = strtok(line, " ");
	    do {
		oldnums[n++] = atoi(st);
	    } while
		((st = strtok((char *)NULL, " ")) != 0);
	    break;

	case 'n':	/* use following letters as text of new lines */
	    for (n = 0; n < screen_lines; n++)
		newtext[n][0] = '.';
	    for (n = 0; n < screen_lines; n++)
		if (line[n+1] == '\n')
		    break;
		else
		    newtext[n][0] = line[n+1];
	    break;

	case 'o':	/* use following letters as text of old lines */
	    for (n = 0; n < screen_lines; n++)
		oldtext[n][0] = '.';
	    for (n = 0; n < screen_lines; n++)
		if (line[n+1] == '\n')
		    break;
		else
		    oldtext[n][0] = line[n+1];
	    break;

	case 'd':	/* dump state of test arrays */
#ifdef TRACE
	    _nc_linedump();
#endif
	    (void) fputs("Old lines: [", stdout);
	    for (n = 0; n < screen_lines; n++)
		putchar(oldtext[n][0]);
	    putchar(']');
	    putchar('\n');
	    (void) fputs("New lines: [", stdout);
	    for (n = 0; n < screen_lines; n++)
		putchar(newtext[n][0]);
	    putchar(']');
	    putchar('\n');
	    break;

	case 'h':	/* apply hash mapper and see scroll optimization */
	    _nc_hash_map();
	    (void) fputs("Result:\n", stderr);
#ifdef TRACE
	    _nc_linedump();
#endif
	    _nc_scroll_optimize();
	    (void) fputs("Done.\n", stderr);
	    break;
	}
    }
    return EXIT_SUCCESS;
}

#endif /* HASHDEBUG */

/* hashmap.c ends here */
