/*	$OpenBSD$	*/

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
 *	infocmp.c -- decompile an entry, or compare two entries
 *		written by Eric S. Raymond
 */

#include <progs.priv.h>

#include <term_entry.h>
#include <dump_entry.h>

MODULE_ID("$From: infocmp.c,v 1.38 1998/10/17 21:32:36 tom Exp $")

#define L_CURL "{"
#define R_CURL "}"

#define MAXTERMS	32	/* max # terminal arguments we can handle */

const char *_nc_progname = "infocmp";

typedef char	path[PATH_MAX];

/***************************************************************************
 *
 * The following control variables, together with the contents of the
 * terminfo entries, completely determine the actions of the program.
 *
 ***************************************************************************/

static char *tname[MAXTERMS];	/* terminal type names */
static TERMTYPE term[MAXTERMS];	/* terminfo entries */
static int termcount;		/* count of terminal entries */

static const char *tversion;	/* terminfo version selected */
static bool numbers = TRUE;	/* format "%'char'" to "%{number}" */
static int outform;		/* output format */
static int sortmode;		/* sort_mode */
static int itrace;		/* trace flag for debugging */
static int mwidth = 60;

/* main comparison mode */
static int compare;
#define C_DEFAULT	0	/* don't force comparison mode */
#define C_DIFFERENCE	1	/* list differences between two terminals */
#define C_COMMON	2	/* list common capabilities */
#define C_NAND		3	/* list capabilities in neither terminal */
#define C_USEALL	4	/* generate relative use-form entry */
static bool ignorepads;		/* ignore pad prefixes when diffing */

#if NO_LEAKS
#undef ExitProgram
static void ExitProgram(int code) GCC_NORETURN;
static void ExitProgram(int code)
{
	while (termcount-- > 0)
		_nc_free_termtype(&term[termcount], FALSE);
	_nc_leaks_dump_entry();
	_nc_free_and_exit(code);
}
#endif

static char *canonical_name(char *ptr, char *buf)
/* extract the terminal type's primary name */
{
    char	*bp;

    (void) strcpy(buf, ptr);
    if ((bp = strchr(buf, '|')) != (char *)NULL)
	*bp = '\0';

    return(buf);
}

/***************************************************************************
 *
 * Predicates for dump function
 *
 ***************************************************************************/

static int capcmp(const char *s, const char *t)
/* capability comparison function */
{
    if (!VALID_STRING(s) && !VALID_STRING(t))
	return(0);
    else if (!VALID_STRING(s) || !VALID_STRING(t))
	return(1);

    if (ignorepads)
	return(_nc_capcmp(s, t));
    else
	return(strcmp(s, t));
}

static int use_predicate(int type, int idx)
/* predicate function to use for use decompilation */
{
	TERMTYPE *tp;

	switch(type)
	{
	case BOOLEAN: {
		int is_set = FALSE;

		/*
		 * This assumes that multiple use entries are supposed
		 * to contribute the logical or of their boolean capabilities.
		 * This is true if we take the semantics of multiple uses to
		 * be 'each capability gets the first non-default value found
		 * in the sequence of use entries'.
		 */
		for (tp = &term[1]; tp < term + termcount; tp++)
			if (tp->Booleans[idx]) {
				is_set = TRUE;
				break;
			}
			if (is_set != term->Booleans[idx])
				return(!is_set);
			else
				return(FAIL);
		}

	case NUMBER: {
		int	value = ABSENT_NUMERIC;

		/*
		 * We take the semantics of multiple uses to be 'each
		 * capability gets the first non-default value found
		 * in the sequence of use entries'.
		 */
		for (tp = &term[1]; tp < term + termcount; tp++)
			if (tp->Numbers[idx] >= 0) {
				value = tp->Numbers[idx];
				break;
			}

		if (value != term->Numbers[idx])
			return(value != ABSENT_NUMERIC);
		else
			return(FAIL);
		}

	case STRING: {
		char *termstr, *usestr = ABSENT_STRING;

		termstr = term->Strings[idx];

		/*
		 * We take the semantics of multiple uses to be 'each
		 * capability gets the first non-default value found
		 * in the sequence of use entries'.
		 */
		for (tp = &term[1]; tp < term + termcount; tp++)
			if (tp->Strings[idx])
			{
				usestr = tp->Strings[idx];
				break;
			}

		if (usestr == ABSENT_STRING && termstr == ABSENT_STRING)
			return(FAIL);
		else if (!usestr || !termstr || capcmp(usestr, termstr))
			return(TRUE);
		else
			return(FAIL);
	    }
	}

	return(FALSE);	/* pacify compiler */
}

static bool entryeq(TERMTYPE *t1, TERMTYPE *t2)
/* are two terminal types equal */
{
    int	i;

    for (i = 0; i < BOOLCOUNT; i++)
	if (t1->Booleans[i] != t2->Booleans[i])
	    return(FALSE);

    for (i = 0; i < NUMCOUNT; i++)
	if (t1->Numbers[i] != t2->Numbers[i])
	    return(FALSE);

    for (i = 0; i < STRCOUNT; i++)
	if (capcmp(t1->Strings[i], t2->Strings[i]))
	    return(FALSE);

    return(TRUE);
}

#define TIC_EXPAND(result) _nc_tic_expand(result, outform==F_TERMINFO, numbers)

static void compare_predicate(int type, int idx, const char *name)
/* predicate function to use for entry difference reports */
{
	register TERMTYPE *t1 = &term[0];
	register TERMTYPE *t2 = &term[1];
	char *s1, *s2;

	switch(type)
	{
	case BOOLEAN:
		switch(compare)
		{
		case C_DIFFERENCE:
			if (t1->Booleans[idx] != t2->Booleans[idx])
			(void) printf("\t%s: %c:%c.\n",
					  name,
					  t1->Booleans[idx] ? 'T' : 'F',
					  t2->Booleans[idx] ? 'T' : 'F');
			break;

		case C_COMMON:
			if (t1->Booleans[idx] && t2->Booleans[idx])
			(void) printf("\t%s= T.\n", name);
			break;

		case C_NAND:
			if (!t1->Booleans[idx] && !t2->Booleans[idx])
			(void) printf("\t!%s.\n", name);
			break;
		}
		break;

	case NUMBER:
		switch(compare)
		{
		case C_DIFFERENCE:
			if (t1->Numbers[idx] != t2->Numbers[idx])
			(void) printf("\t%s: %d:%d.\n",
					  name, t1->Numbers[idx], t2->Numbers[idx]);
			break;

		case C_COMMON:
			if (t1->Numbers[idx]!=-1 && t2->Numbers[idx]!=-1
				&& t1->Numbers[idx] == t2->Numbers[idx])
			(void) printf("\t%s= %d.\n", name, t1->Numbers[idx]);
			break;

		case C_NAND:
			if (t1->Numbers[idx]==-1 && t2->Numbers[idx] == -1)
			(void) printf("\t!%s.\n", name);
			break;
		}
	break;

	case STRING:
		s1 = t1->Strings[idx];
		s2 = t2->Strings[idx];
		switch(compare)
		{
		case C_DIFFERENCE:
			if (capcmp(s1, s2))
			{
				char	buf1[BUFSIZ], buf2[BUFSIZ];

				if (s1 == (char *)NULL)
					(void) strcpy(buf1, "NULL");
				else
				{
					(void) strcpy(buf1, "'");
					(void) strcat(buf1, TIC_EXPAND(s1));
					(void) strcat(buf1, "'");
				}

				if (s2 == (char *)NULL)
					(void) strcpy(buf2, "NULL");
				else
				{
					(void) strcpy(buf2, "'");
					(void) strcat(buf2, TIC_EXPAND(s2));
					(void) strcat(buf2, "'");
				}

				if (strcmp(buf1, buf2))
					(void) printf("\t%s: %s, %s.\n",
						      name, buf1, buf2);
			}
			break;

		case C_COMMON:
			if (s1 && s2 && !capcmp(s1, s2))
				(void) printf("\t%s= '%s'.\n", name, TIC_EXPAND(s1));
			break;

		case C_NAND:
			if (!s1 && !s2)
				(void) printf("\t!%s.\n", name);
			break;
		}
		break;
	}

}

/***************************************************************************
 *
 * Init string analysis
 *
 ***************************************************************************/

typedef struct {const char *from; const char *to;} assoc;

static const assoc std_caps[] =
{
    /* these are specified by X.364 and iBCS2 */
    {"\033c",	"RIS"},		/* full reset */
    {"\0337",	"SC"},		/* save cursor */
    {"\0338",	"RC"},		/* restore cursor */
    {"\033[r",	"RSR"},		/* not an X.364 mnemonic */
    {"\033[m",	"SGR0"},	/* not an X.364 mnemonic */
    {"\033[2J",	"ED2"},		/* clear page */

    /* this group is specified by ISO 2022 */
    {"\033(0",	"ISO DEC G0"},	/* enable DEC graphics for G0 */
    {"\033(A",	"ISO UK G0"},	/* enable UK chars for G0 */
    {"\033(B",	"ISO US G0"},	/* enable US chars for G0 */
    {"\033)0",	"ISO DEC G1"},	/* enable DEC graphics for G1 */
    {"\033)A",	"ISO UK G1"},	/* enable UK chars for G1 */
    {"\033)B",	"ISO US G1"},	/* enable US chars for G1 */

    /* these are DEC private modes widely supported by emulators */
    {"\033=",	"DECPAM"},	/* application keypad mode */
    {"\033>",	"DECPNM"},	/* normal keypad mode */
    {"\033<",	"DECANSI"},	/* enter ANSI mode */

    { (char *)0, (char *)0}
};

static const assoc private_modes[] =
/* DEC \E[ ... [hl] modes recognized by many emulators */
{
    {"1",	"CKM"},		/* application cursor keys */
    {"2",	"ANM"},		/* set VT52 mode */
    {"3",	"COLM"},	/* 132-column mode */
    {"4",	"SCLM"},	/* smooth scroll */
    {"5",	"SCNM"},	/* reverse video mode */
    {"6",	"OM"},		/* origin mode */
    {"7",	"AWM"},		/* wraparound mode */
    {"8",	"ARM"},		/* auto-repeat mode */
    {(char *)0, (char *)0}
};

static const assoc ecma_highlights[] =
/* recognize ECMA attribute sequences */
{
    {"0",	"NORMAL"},	/* normal */
    {"1",	"+BOLD"},	/* bold on */
    {"2",	"+DIM"},	/* dim on */
    {"3",	"+ITALIC"},	/* italic on */
    {"4",	"+UNDERLINE"},	/* underline on */
    {"5",	"+BLINK"},	/* blink on */
    {"6",	"+FASTBLINK"},	/* fastblink on */
    {"7",	"+REVERSE"},	/* reverse on */
    {"8",	"+INVISIBLE"},	/* invisible on */
    {"9",	"+DELETED"},	/* deleted on */
    {"10",	"MAIN-FONT"},	/* select primary font */
    {"11",	"ALT-FONT-1"},	/* select alternate font 1 */
    {"12",	"ALT-FONT-2"},	/* select alternate font 2 */
    {"13",	"ALT-FONT-3"},	/* select alternate font 3 */
    {"14",	"ALT-FONT-4"},	/* select alternate font 4 */
    {"15",	"ALT-FONT-5"},	/* select alternate font 5 */
    {"16",	"ALT-FONT-6"},	/* select alternate font 6 */
    {"17",	"ALT-FONT-7"},	/* select alternate font 7 */
    {"18",	"ALT-FONT-1"},	/* select alternate font 1 */
    {"19",	"ALT-FONT-1"},	/* select alternate font 1 */
    {"20",	"FRAKTUR"},	/* Fraktur font */
    {"21",	"DOUBLEUNDER"},	/* double underline */
    {"22",	"-DIM"},	/* dim off */
    {"23",	"-ITALIC"},	/* italic off */
    {"24",	"-UNDERLINE"},	/* underline off */
    {"25",	"-BLINK"},	/* blink off */
    {"26",	"-FASTBLINK"},	/* fastblink off */
    {"27",	"-REVERSE"},	/* reverse off */
    {"28",	"-INVISIBLE"},	/* invisible off */
    {"29",	"-DELETED"},	/* deleted off */
    {(char *)0, (char *)0}
};

static void analyze_string(const char *name, const char *cap, TERMTYPE *tp)
{
    char	buf[MAX_TERMINFO_LENGTH];
    char	buf2[MAX_TERMINFO_LENGTH];
    const char	*sp, *ep;
    const assoc	*ap;

    if (cap == ABSENT_STRING || cap == CANCELLED_STRING)
	return;
    (void) printf("%s: ", name);

    buf[0] = '\0';
    for (sp = cap; *sp; sp++)
    {
	int	i;
	size_t	len = 0;
	const char *expansion = 0;

	/* first, check other capabilities in this entry */
	for (i = 0; i < STRCOUNT; i++)
	{
	    char	*cp = tp->Strings[i];

	    /* don't use soft-key capabilities */
	    if (strnames[i][0] == 'k' && strnames[i][0] == 'f')
		continue;


	    if (cp != ABSENT_STRING && cp != CANCELLED_STRING && cp[0] && cp != cap)
	    {
		len = strlen(cp);
		(void) strncpy(buf2, sp, len);
		buf2[len] = '\0';

		if (_nc_capcmp(cp, buf2))
		    continue;

#define ISRS(s)	(!strncmp((s), "is", 2) || !strncmp((s), "rs", 2))
		/*
		 * Theoretically we just passed the test for translation
		 * (equality once the padding is stripped).  However, there
		 * are a few more hoops that need to be jumped so that
		 * identical pairs of initialization and reset strings
		 * don't just refer to each other.
		 */
		if (ISRS(name) || ISRS(strnames[i]))
		    if (cap < cp)
			continue;
#undef ISRS

		expansion = strnames[i];
		break;
	    }
	}

	/* now check the standard capabilities */
	if (!expansion)
	    for (ap = std_caps; ap->from; ap++)
	    {
		len = strlen(ap->from);

		if (strncmp(ap->from, sp, len) == 0)
		{
		    expansion = ap->to;
		    break;
		}
	    }

	/* now check for private-mode sequences */
	if (!expansion
		    && sp[0] == '\033' && sp[1] == '[' && sp[2] == '?'
		    && (len = strspn(sp + 3, "0123456789;"))
		    && ((sp[3 + len] == 'h') || (sp[3 + len] == 'l')))
	{
	    char	buf3[MAX_TERMINFO_LENGTH];

	    (void) strcpy(buf2, (sp[3 + len] == 'h') ? "DEC+" : "DEC-");
	    (void) strncpy(buf3, sp + 3, len);
	    len += 4;
	    buf3[len] = '\0';

	    ep = strtok(buf3, ";");
	    do {
		   bool	found = FALSE;

		   for (ap = private_modes; ap->from; ap++)
		   {
		       size_t tlen = strlen(ap->from);

		       if (strncmp(ap->from, ep, tlen) == 0)
		       {
			   (void) strcat(buf2, ap->to);
			   found = TRUE;
			   break;
		       }
		   }

		   if (!found)
		       (void) strcat(buf2, ep);
		   (void) strcat(buf2, ";");
	       } while
		   ((ep = strtok((char *)NULL, ";")));
	    buf2[strlen(buf2) - 1] = '\0';
	    expansion = buf2;
	}

	/* now check for ECMA highlight sequences */
	if (!expansion
		    && sp[0] == '\033' && sp[1] == '['
		    && (len = strspn(sp + 2, "0123456789;"))
		    && sp[2 + len] == 'm')
	{
	    char	buf3[MAX_TERMINFO_LENGTH];

	    (void) strcpy(buf2, "SGR:");
	    (void) strncpy(buf3, sp + 2, len);
	    len += 3;
	    buf3[len] = '\0';

	    ep = strtok(buf3, ";");
	    do {
		   bool	found = FALSE;

		   for (ap = ecma_highlights; ap->from; ap++)
		   {
		       size_t tlen = strlen(ap->from);

		       if (strncmp(ap->from, ep, tlen) == 0)
		       {
			   (void) strcat(buf2, ap->to);
			   found = TRUE;
			   break;
		       }
		   }

		   if (!found)
		       (void) strcat(buf2, ep);
		   (void) strcat(buf2, ";");
	       } while
		   ((ep = strtok((char *)NULL, ";")));

	    buf2[strlen(buf2) - 1] = '\0';
	    expansion = buf2;
	}
	/* now check for scroll region reset */
	if (!expansion)
	{
	    (void) sprintf(buf2, "\033[1;%dr", tp->Numbers[2]);
	    len = strlen(buf2);
	    if (strncmp(buf2, sp, len) == 0)
		expansion = "RSR";
	}

	/* now check for home-down */
	if (!expansion)
	{
	    (void) sprintf(buf2, "\033[%d;1H", tp->Numbers[2]);
	    len = strlen(buf2);
	    if (strncmp(buf2, sp, len) == 0)
		    expansion = "LL";
	}

	/* now look at the expansion we got, if any */
	if (expansion)
	{
	    (void) sprintf(buf + strlen(buf), "{%s}", expansion);
	    sp += len - 1;
	    continue;
	}
	else
	{
	    /* couldn't match anything */
	    buf2[0] = *sp;
	    buf2[1] = '\0';
	    (void) strcat(buf, TIC_EXPAND(buf2));
	}
    }
    (void) printf("%s\n", buf);
}

/***************************************************************************
 *
 * File comparison
 *
 ***************************************************************************/

static void file_comparison(int argc, char *argv[])
{
#define MAXCOMPARE	2
    /* someday we may allow comparisons on more files */
    int	filecount = 0;
    ENTRY	*heads[MAXCOMPARE];
    ENTRY	*tails[MAXCOMPARE];
    ENTRY	*qp, *rp;
    int		i, n;

    dump_init((char *)NULL, F_LITERAL, S_TERMINFO, 0, itrace, FALSE);

    for (n = 0; n < argc && n < MAXCOMPARE; n++)
    {
	if (freopen(argv[n], "r", stdin) == NULL)
	    _nc_err_abort("Can't open %s", argv[n]);

	_nc_head = _nc_tail = (ENTRY *)NULL;

	/* parse entries out of the source file */
	_nc_set_source(argv[n]);
	_nc_read_entry_source(stdin, NULL, TRUE, FALSE, NULLHOOK);

	if (itrace)
	    (void) fprintf(stderr, "Resolving file %d...\n", n-0);

	/* do use resolution */
	if (!_nc_resolve_uses())
	{
	    (void) fprintf(stderr,
			   "There are unresolved use entries in %s:\n",
			   argv[n]);
	    for_entry_list(qp)
		if (qp->nuses)
		{
		    (void) fputs(qp->tterm.term_names, stderr);
		    (void) fputc('\n', stderr);
		}
	    exit(EXIT_FAILURE);
	}

	heads[filecount] = _nc_head;
	tails[filecount] = _nc_tail;
	filecount++;
    }

    /* OK, all entries are in core.  Ready to do the comparison */
    if (itrace)
	(void) fprintf(stderr, "Entries are now in core...\n");

    /*
     * The entry-matching loop.  We're not using the use[]
     * slots any more (they got zeroed out by resolve_uses) so
     * we stash each entry's matches in the other file there.
     * Sigh, this is intrinsically quadratic.
     */
    for (qp = heads[0]; qp; qp = qp->next)
    {
	for (rp = heads[1]; rp; rp = rp->next)
	    if (_nc_entry_match(qp->tterm.term_names, rp->tterm.term_names))
	    {
		/*
		 * This is why the uses structure parent element is
		 * (void *) -- so we can have either (char *) for
		 * names or entry structure pointers in them and still
		 * be type-safe.
		 */
		if (qp->nuses < MAX_USES)
		    qp->uses[qp->nuses].parent = (void *)rp;
		qp->nuses++;

		if (rp->nuses < MAX_USES)
		    rp->uses[rp->nuses].parent = (void *)qp;
		rp->nuses++;
	    }
    }

    /* now we have two circular lists with crosslinks */
    if (itrace)
	(void) fprintf(stderr, "Name matches are done...\n");

    for (qp = heads[0]; qp; qp = qp->next)
	if (qp->nuses > 1)
	{
	    (void) fprintf(stderr,
			   "%s in file 1 (%s) has %d matches in file 2 (%s):\n",
			   _nc_first_name(qp->tterm.term_names),
			   argv[0],
			   qp->nuses,
			   argv[1]);
	    for (i = 0; i < qp->nuses; i++)
		(void) fprintf(stderr,
			       "\t%s\n",
			       _nc_first_name(((ENTRY *)qp->uses[i].parent)->tterm.term_names));
	}
    for (rp = heads[1]; rp; rp = rp->next)
	if (rp->nuses > 1)
	{
	    (void) fprintf(stderr,
			   "%s in file 2 (%s) has %d matches in file 1 (%s):\n",
			   _nc_first_name(rp->tterm.term_names),
			   argv[1],
			   rp->nuses,
			   argv[0]);
	    for (i = 0; i < rp->nuses; i++)
		(void) fprintf(stderr,
			       "\t%s\n",
			       _nc_first_name(((ENTRY *)rp->uses[i].parent)->tterm.term_names));
	}

    (void) printf("In file 1 (%s) only:\n", argv[0]);
    for (qp = heads[0]; qp; qp = qp->next)
	if (qp->nuses == 0)
	    (void) printf("\t%s\n",
			  _nc_first_name(qp->tterm.term_names));

    (void) printf("In file 2 (%s) only:\n", argv[1]);
    for (rp = heads[1]; rp; rp = rp->next)
	if (rp->nuses == 0)
	    (void) printf("\t%s\n",
			  _nc_first_name(rp->tterm.term_names));

    (void) printf("The following entries are equivalent:\n");
    for (qp = heads[0]; qp; qp = qp->next)
    {
	rp = (ENTRY *)qp->uses[0].parent;

	if (qp->nuses == 1 && entryeq(&qp->tterm, &rp->tterm))
	{
	    char name1[NAMESIZE], name2[NAMESIZE];

	    (void) canonical_name(qp->tterm.term_names, name1);
	    (void) canonical_name(rp->tterm.term_names, name2);

	    (void) printf("%s = %s\n", name1, name2);
	}
    }

    (void) printf("Differing entries:\n");
    termcount = 2;
    for (qp = heads[0]; qp; qp = qp->next)
    {
	rp = (ENTRY *)qp->uses[0].parent;

	if (qp->nuses == 1 && !entryeq(&qp->tterm, &rp->tterm))
	{
	    char name1[NAMESIZE], name2[NAMESIZE];

	    memcpy(&term[0], &qp->tterm, sizeof(TERMTYPE));
	    memcpy(&term[1], &rp->tterm, sizeof(TERMTYPE));

	    (void) canonical_name(qp->tterm.term_names, name1);
	    (void) canonical_name(rp->tterm.term_names, name2);

	    switch (compare)
	    {
	    case C_DIFFERENCE:
		if (itrace)
		    (void)fprintf(stderr, "infocmp: dumping differences\n");
		(void) printf("comparing %s to %s.\n", name1, name2);
		compare_entry(compare_predicate);
		break;

	    case C_COMMON:
		if (itrace)
		    (void) fprintf(stderr,
				   "infocmp: dumping common capabilities\n");
		(void) printf("comparing %s to %s.\n", name1, name2);
		compare_entry(compare_predicate);
		break;

	    case C_NAND:
		if (itrace)
		    (void) fprintf(stderr,
				   "infocmp: dumping differences\n");
		(void) printf("comparing %s to %s.\n", name1, name2);
		compare_entry(compare_predicate);
		break;

	    }
	}
    }
}

static void usage(void)
{
	static const char *tbl[] = {
	     "Usage: infocmp [options] [-A directory] [-B directory] [termname...]"
	    ,""
	    ,"Options:"
	    ,"  -1    print single-column"
	    ,"  -C    use termcap-names"
	    ,"  -F    compare terminfo-files"
	    ,"  -I    use terminfo-names"
	    ,"  -L    use long names"
	    ,"  -R subset (see manpage)"
	    ,"  -T    eliminate size limits (test)"
	    ,"  -V    print version"
	    ,"  -c    list common capabilities"
	    ,"  -d    list different capabilities"
	    ,"  -e    format output as C initializer"
	    ,"  -f    with -1, format complex strings"
	    ,"  -g    format %'char' to %{number}"
	    ,"  -i    analyze initialization/reset"
	    ,"  -l    output terminfo names"
	    ,"  -n    list capabilities in neither"
	    ,"  -p    ignore padding specifiers"
	    ,"  -r    with -C, output in termcap form"
	    ,"  -s [d|i|l|c] sort fields"
	    ,"  -u    produce source with 'use='"
	    ,"  -v number  (verbose)"
	    ,"  -w number  (width)"
	};
	const size_t first = 3;
	const size_t last = sizeof(tbl)/sizeof(tbl[0]);
	const size_t left = (last - first + 1) / 2 + first;
	size_t n;

	for (n = 0; n < left; n++) {
		size_t m = (n < first) ? last : n + left - first;
		if (m < last)
			fprintf(stderr, "%-40.40s%s\n", tbl[n], tbl[m]);
		else
			fprintf(stderr, "%s\n", tbl[n]);
	}
	exit(EXIT_FAILURE);
}

/***************************************************************************
 *
 * Main sequence
 *
 ***************************************************************************/

int main(int argc, char *argv[])
{
	char *terminal, *firstdir, *restdir;
	/* Avoid "local data >32k" error with mwcc */
	/* Also avoid overflowing smaller stacks on systems like AmigaOS */
	path *tfile = malloc(sizeof(path)*MAXTERMS);
	int c, i, len;
	bool formatted = FALSE;
	bool filecompare = FALSE;
	bool initdump = FALSE;
	bool init_analyze = FALSE;
	bool limited = TRUE;

	if ((terminal = getenv("TERM")) == NULL)
	{
		(void) fprintf(stderr,
			"infocmp: environment variable TERM not set\n");
		return EXIT_FAILURE;
	}

	/* where is the terminfo database location going to default to? */
	restdir = firstdir = 0;

	while ((c = getopt(argc, argv, "decCfFgIinlLprR:s:uv:Vw:A:B:1T")) != EOF)
		switch (c)
		{
		case 'd':
			compare = C_DIFFERENCE;
			break;

		case 'e':
			initdump = TRUE;
			break;

		case 'c':
			compare = C_COMMON;
			break;

		case 'C':
			outform = F_TERMCAP;
			tversion = "BSD";
			if (sortmode == S_DEFAULT)
			    sortmode = S_TERMCAP;
			break;

		case 'f':
			formatted = TRUE;
			break;

		case 'g':
			numbers = FALSE;
			break;

		case 'F':
			filecompare = TRUE;
			break;

		case 'I':
			outform = F_TERMINFO;
			if (sortmode == S_DEFAULT)
			    sortmode = S_VARIABLE;
			tversion = 0;
			break;

		case 'i':
			init_analyze = TRUE;
			break;

		case 'l':
			outform = F_TERMINFO;
			break;

		case 'L':
			outform = F_VARIABLE;
			if (sortmode == S_DEFAULT)
			    sortmode = S_VARIABLE;
			break;

		case 'n':
			compare = C_NAND;
			break;

		case 'p':
			ignorepads = TRUE;
			break;

		case 'r':
			tversion = 0;
			limited = FALSE;
			break;

		case 'R':
			tversion = optarg;
			break;

		case 's':
			if (*optarg == 'd')
				sortmode = S_NOSORT;
			else if (*optarg == 'i')
				sortmode = S_TERMINFO;
			else if (*optarg == 'l')
				sortmode = S_VARIABLE;
			else if (*optarg == 'c')
				sortmode = S_TERMCAP;
			else
			{
				(void) fprintf(stderr,
					       "infocmp: unknown sort mode\n");
				return EXIT_FAILURE;
			}
			break;

		case 'u':
			compare = C_USEALL;
			break;

		case 'v':
			itrace = atoi(optarg);
			_nc_tracing = (1 << itrace) - 1;
			break;

		case 'V':
			(void) fputs(NCURSES_VERSION, stdout);
			putchar('\n');
			ExitProgram(EXIT_SUCCESS);

		case 'w':
			mwidth = atoi(optarg);
			break;

		case 'A':
			firstdir = optarg;
			break;

		case 'B':
			restdir = optarg;
			break;

		case '1':
			mwidth = 0;
			break;

		case 'T':
			limited = FALSE;
			break;
		default:
			usage();
		}

	/* by default, sort by terminfo name */
	if (sortmode == S_DEFAULT)
		sortmode = S_TERMINFO;

	/* set up for display */
	dump_init(tversion, outform, sortmode, mwidth, itrace, formatted);

	/* make sure we have at least one terminal name to work with */
	if (optind >= argc)
		argv[argc++] = terminal;

	/* if user is after a comparison, make sure we have two entries */
	if (compare != C_DEFAULT && optind >= argc - 1)
		argv[argc++] = terminal;

	/* exactly two terminal names with no options means do -d */
	if (argc - optind == 2 && compare == C_DEFAULT)
		compare = C_DIFFERENCE;

	if (!filecompare)
	{
	    /* grab the entries */
	    termcount = 0;
	    for (; optind < argc; optind++)
	    {
		if (termcount >= MAXTERMS)
		{
		    (void) fprintf(stderr,
			   "infocmp: too many terminal type arguments\n");
		    return EXIT_FAILURE;
		}
		else
		{
		    const char	*directory = termcount ? restdir : firstdir;
		    int		status;

		    tname[termcount] = argv[optind];

		    if (directory)
		    {
			(void) sprintf(tfile[termcount], "%s/%c/%s",
				       directory,
				       *argv[optind], argv[optind]);
			if (itrace)
			    (void) fprintf(stderr,
					   "infocmp: reading entry %s from file %s\n",
					   argv[optind], tfile[termcount]);

			status = _nc_read_file_entry(tfile[termcount],
						     &term[termcount]);
		    }
		    else
		    {
			if (itrace)
			    (void) fprintf(stderr,
					   "infocmp: reading entry %s from system directories %s\n",
					   argv[optind], tname[termcount]);

			status = _nc_read_entry(tname[termcount],
						tfile[termcount],
						&term[termcount]);
			directory = TERMINFO;	/* for error message */
		    }

		    if (status <= 0)
		    {
			(void) fprintf(stderr,
				       "infocmp: couldn't open terminfo file %s.\n",
				       tfile[termcount]);
			return EXIT_FAILURE;
		    }
		    termcount++;
		}
	    }

	    /* dump as C initializer for the terminal type */
	    if (initdump)
	    {
		int	n;
		const char *str = 0;
		int	size;

		(void) printf("\t%s\n\t\t\"%s\",\n",
			      L_CURL, term->term_names);
		(void) printf("\t\t(char *)0,\n");

		(void) printf("\t\t%s /* BOOLEANS */\n", L_CURL);
		for (n = 0; n < BOOLCOUNT; n++)
		{
		    switch((int)(term->Booleans[n]))
		    {
		    case TRUE:
			str = "TRUE";
			break;

		    case FALSE:
			str = "FALSE";
			break;

		    case ABSENT_BOOLEAN:
			str = "ABSENT_BOOLEAN";
			break;

		    case CANCELLED_BOOLEAN:
			str = "CANCELLED_BOOLEAN";
			break;
		    }
		    (void) printf("\t\t/* %s */\t%s%s,\n",
				  boolnames[n], str,
				  n == BOOLCOUNT-1 ? R_CURL : "");
		}

		(void) printf("\t\t%s /* NUMERICS */\n", L_CURL);
		for (n = 0; n < NUMCOUNT; n++)
		{
		    char	buf[BUFSIZ];
		    switch (term->Numbers[n])
		    {
		    case ABSENT_NUMERIC:
			str = "ABSENT_NUMERIC";
			break;
		    case CANCELLED_NUMERIC:
			str = "CANCELLED_NUMERIC";
			break;
		    default:
			sprintf(buf, "%d", term->Numbers[n]);
			str = buf;
			break;
		    }
		    (void) printf("\t\t/* %s */\t%s%s,\n",
			numnames[n], str,
			n == NUMCOUNT-1 ? R_CURL : "");
		}

		size = sizeof(TERMTYPE)
		    + (BOOLCOUNT * sizeof(term->Booleans[0]))
		    + (NUMCOUNT * sizeof(term->Numbers[0]));

		(void) printf("\t\t%s /* STRINGS */\n", L_CURL);
		for (n = 0; n < STRCOUNT; n++)
		{
		    char	buf[BUFSIZ], *sp, *tp;

		    if (term->Strings[n] == ABSENT_STRING)
			str = "ABSENT_STRING";
		    else if (term->Strings[n] == CANCELLED_STRING)
			str = "CANCELLED_STRING";
		    else
		    {
			tp = buf;
			*tp++ = '"';
			for (sp = term->Strings[n]; *sp; sp++)
			{
			    if (isascii(*sp) && isprint(*sp) && *sp !='\\' && *sp != '"')
				*tp++ = *sp;
			    else
			    {
				(void) sprintf(tp, "\\%03o", *sp & 0xff);
				tp += 4;
			    }
			}
			*tp++ = '"';
			*tp = '\0';
			size += (strlen(term->Strings[n]) + 1);
			str = buf;
		    }
		    (void) printf("\t\t/* %s */\t%s%s%s\n",
		    	strnames[n], str,
			n == STRCOUNT-1 ? R_CURL : "",
			n == STRCOUNT-1 ? ""     : ",");
		}
		(void) printf("\t%s /* size = %d */\n", R_CURL, size);
		ExitProgram(EXIT_SUCCESS);
	    }

	    /* analyze the init strings */
	    if (init_analyze)
	    {
#undef CUR
#define CUR	term[0].
		analyze_string("is1", init_1string, &term[0]);
		analyze_string("is2", init_2string, &term[0]);
		analyze_string("is3", init_3string, &term[0]);
		analyze_string("rs1", reset_1string, &term[0]);
		analyze_string("rs2", reset_2string, &term[0]);
		analyze_string("rs3", reset_3string, &term[0]);
		analyze_string("smcup", enter_ca_mode, &term[0]);
		analyze_string("rmcup", exit_ca_mode, &term[0]);
#undef CUR
		ExitProgram(EXIT_SUCCESS);
	    }

	    /*
	     * Here's where the real work gets done
	     */
	    switch (compare)
	    {
	    case C_DEFAULT:
		if (itrace)
		    (void) fprintf(stderr,
				   "infocmp: about to dump %s\n",
				   tname[0]);
		(void) printf("#\tReconstructed via infocmp from file: %s\n",
			      tfile[0]);
		len = dump_entry(&term[0], limited, numbers, NULL);
		putchar('\n');
		if (itrace)
		    (void)fprintf(stderr, "infocmp: length %d\n", len);
		break;

	    case C_DIFFERENCE:
		if (itrace)
		    (void)fprintf(stderr, "infocmp: dumping differences\n");
		(void) printf("comparing %s to %s.\n", tname[0], tname[1]);
		compare_entry(compare_predicate);
		break;

	    case C_COMMON:
		if (itrace)
		    (void) fprintf(stderr,
				   "infocmp: dumping common capabilities\n");
		(void) printf("comparing %s to %s.\n", tname[0], tname[1]);
		compare_entry(compare_predicate);
		break;

	    case C_NAND:
		if (itrace)
		    (void) fprintf(stderr,
				   "infocmp: dumping differences\n");
		(void) printf("comparing %s to %s.\n", tname[0], tname[1]);
		compare_entry(compare_predicate);
		break;

	    case C_USEALL:
		if (itrace)
		    (void) fprintf(stderr, "infocmp: dumping use entry\n");
		len = dump_entry(&term[0], limited, numbers, use_predicate);
		for (i = 1; i < termcount; i++)
		    len += dump_uses(tname[i], !(outform==F_TERMCAP || outform==F_TCONVERR));
		putchar('\n');
		if (itrace)
		    (void)fprintf(stderr, "infocmp: length %d\n", len);
		break;
	    }
	}
	else if (compare == C_USEALL)
	    (void) fprintf(stderr, "Sorry, -u doesn't work with -F\n");
	else if (compare == C_DEFAULT)
	    (void) fprintf(stderr, "Use `tic -[CI] <file>' for this.\n");
	else if (argc - optind != 2)
	    (void) fprintf(stderr,
		"File comparison needs exactly two file arguments.\n");
	else
	    file_comparison(argc-optind, argv+optind);

	ExitProgram(EXIT_SUCCESS);
}

/* infocmp.c ends here */
