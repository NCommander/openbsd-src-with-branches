/*	$OpenBSD: digraph.c,v 1.1.1.1 1996/09/07 21:40:26 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * digraph.c: code for digraphs
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"

#ifdef DIGRAPHS

static int getexactdigraph __ARGS((int, int, int));
static void printdigraph __ARGS((char_u *));

static char_u	(*digraphnew)[3];			/* pointer to added digraphs */
static int		digraphcount = 0;			/* number of added digraphs */

#if defined(MSDOS) || defined(WIN32) || defined(OS2)
char_u	digraphdefault[][3] = 		/* standard MSDOS digraphs */
	   {{'C', ',', 128},	/* ~@ (SAS C can't handle the real char) */
		{'u', '"', 129},	/* � */
		{'e', '\'', 130},	/* � */
		{'a', '^', 131},	/* � */
		{'a', '"', 132},	/* � */
		{'a', '`', 133},	/* � */
		{'a', '@', 134},	/* � */
		{'c', ',', 135},	/* ~G (SAS C can't handle the real char) */
		{'e', '^', 136},	/* ~H (SAS C can't handle the real char) */
		{'e', '"', 137},	/* � */
		{'e', '`', 138},	/* � */
		{'i', '"', 139},	/* � */
		{'i', '^', 140},	/* � */
		{'i', '`', 141},	/* � */
		{'A', '"', 142},	/* � */
		{'A', '@', 143},	/* � */
		{'E', '\'', 144},	/* � */
		{'a', 'e', 145},	/* � */
		{'A', 'E', 146},	/* � */
		{'o', '^', 147},	/* � */
		{'o', '"', 148},	/* � */
		{'o', '`', 149},	/* � */
		{'u', '^', 150},	/* � */
		{'u', '`', 151},	/* � */
		{'y', '"', 152},	/* � */
		{'O', '"', 153},	/* � */
		{'U', '"', 154},	/* � */
	    {'c', '|', 155},	/* � */
	    {'$', '$', 156},	/* � */
	    {'Y', '-', 157},	/* ~] (SAS C can't handle the real char) */
	    {'P', 't', 158},	/* � */
	    {'f', 'f', 159},	/* � */
		{'a', '\'', 160},	/* � */
		{'i', '\'', 161},	/* � */
		{'o', '\'', 162},	/* � */
		{'u', '\'', 163},	/* xx (SAS C can't handle the real char) */
		{'n', '~', 164},	/* � */
		{'N', '~', 165},	/* � */
		{'a', 'a', 166},	/* � */
		{'o', 'o', 167},	/* � */
		{'~', '?', 168},	/* � */
		{'-', 'a', 169},	/* � */
		{'a', '-', 170},	/* � */
		{'1', '2', 171},	/* � */
		{'1', '4', 172},	/* � */
		{'~', '!', 173},	/* � */
		{'<', '<', 174},	/* � */
		{'>', '>', 175},	/* � */

		{'s', 's', 225},	/* � */
		{'j', 'u', 230},	/* � */
		{'o', '/', 237},	/* � */
		{'+', '-', 241},	/* � */
		{'>', '=', 242},	/* � */
		{'<', '=', 243},	/* � */
		{':', '-', 246},	/* � */
		{'~', '~', 247},	/* � */
		{'~', 'o', 248},	/* � */
		{'2', '2', 253},	/* � */
		{NUL, NUL, NUL}
		};

#else	/* !MSDOS && !WIN32 */
# ifdef MINT
char_u	digraphdefault[][3] = 		/* standard ATARI digraphs */
	   {{'C', ',', 128},	/* ~@ */
		{'u', '"', 129},	/* � */
		{'e', '\'', 130},	/* � */
		{'a', '^', 131},	/* � */
		{'a', '"', 132},	/* � */
		{'a', '`', 133},	/* � */
		{'a', '@', 134},	/* � */
		{'c', ',', 135},	/* ~G */
		{'e', '^', 136},	/* ~H */
		{'e', '"', 137},	/* � */
		{'e', '`', 138},	/* � */
		{'i', '"', 139},	/* � */
		{'i', '^', 140},	/* � */
		{'i', '`', 141},	/* � */
		{'A', '"', 142},	/* � */
		{'A', '@', 143},	/* � */
		{'E', '\'', 144},	/* � */
		{'a', 'e', 145},	/* � */
		{'A', 'E', 146},	/* � */
		{'o', '^', 147},	/* � */
		{'o', '"', 148},	/* � */
		{'o', '`', 149},	/* � */
		{'u', '^', 150},	/* � */
		{'u', '`', 151},	/* � */
		{'y', '"', 152},	/* � */
		{'O', '"', 153},	/* � */
		{'U', '"', 154},	/* � */
	   	{'c', '|', 155},	/* � */
	   	{'$', '$', 156},	/* � */
	   	{'Y', '-', 157},	/* ~] */
	   	{'s', 's', 158},	/* � */
	    {'f', 'f', 159},	/* � */
		{'a', '\'', 160},	/* � */
		{'i', '\'', 161},	/* � */
		{'o', '\'', 162},	/* � */
		{'u', '\'', 163},	/* � */
		{'n', '~', 164},	/* � */
		{'N', '~', 165},	/* � */
		{'a', 'a', 166},	/* � */
		{'o', 'o', 167},	/* � */
		{'~', '?', 168},	/* � */
		{'-', 'a', 169},	/* � */
		{'a', '-', 170},	/* � */
		{'1', '2', 171},	/* � */
		{'1', '4', 172},	/* � */
		{'~', '!', 173},	/* � */
		{'<', '<', 174},	/* � */
		{'>', '>', 175},	/* � */
		{'j', 'u', 230},	/* � */
		{'o', '/', 237},	/* � */
		{'+', '-', 241},	/* � */
		{'>', '=', 242},	/* � */
		{'<', '=', 243},	/* � */
		{':', '-', 246},	/* � */
		{'~', '~', 247},	/* � */
		{'~', 'o', 248},	/* � */
		{'2', '2', 253},	/* � */
		{NUL, NUL, NUL}
		};

# else	/* !MINT */
#  ifdef _INCLUDE_HPUX_SOURCE

char_u	digraphdefault[][3] = 		/* default HPUX digraphs */
	   {{'A', '`', 161},	/* � */
	    {'A', '^', 162},	/* � */
	    {'E', '`', 163},	/* � */
	    {'E', '^', 164},	/* � */
	    {'E', '"', 165},	/* � */
	    {'I', '^', 166},	/* � */
	    {'I', '"', 167},	/* � */
	    {'\'', '\'', 168},	/* � */
	    {'`', '`', 169},	/* � */
		{'^', '^', 170},	/* � */
		{'"', '"', 171},	/* � */
		{'~', '~', 172},	/* � */
		{'U', '`', 173},	/* � */
		{'U', '^', 174},	/* � */
		{'L', '=', 175},	/* � */
		{'~', '_', 176},	/* � */
		{'Y', '\'', 177},	/* � */
		{'y', '\'', 178},	/* � */
		{'~', 'o', 179},	/* � */
		{'C', ',', 180},	/* � */
		{'c', ',', 181},	/* � */
		{'N', '~', 182},	/* � */
		{'n', '~', 183},	/* � */
		{'~', '!', 184},	/* � */
		{'~', '?', 185},	/* � */
		{'o', 'x', 186},	/* � */
		{'L', '-', 187},	/* � */
		{'Y', '=', 188},	/* � */
		{'p', 'p', 189},	/* � */
		{'f', 'l', 190},	/* � */
		{'c', '|', 191},	/* � */
		{'a', '^', 192},	/* � */
		{'e', '^', 193},	/* � */
		{'o', '^', 194},	/* � */
		{'u', '^', 195},	/* � */
		{'a', '\'', 196},	/* � */
		{'e', '\'', 197},	/* � */
		{'o', '\'', 198},	/* � */
		{'u', '\'', 199},	/* � */
		{'a', '`', 200},	/* � */
		{'e', '`', 201},	/* � */
		{'o', '`', 202},	/* � */
		{'u', '`', 203},	/* � */
		{'a', '"', 204},	/* � */
		{'e', '"', 205},	/* � */
		{'o', '"', 206},	/* � */
		{'u', '"', 207},	/* � */
		{'A', 'o', 208},	/* � */
		{'i', '^', 209},	/* � */
		{'O', '/', 210},	/* � */
		{'A', 'E', 211},	/* � */
		{'a', 'o', 212},	/* � */
		{'i', '\'', 213},	/* � */
		{'o', '/', 214},	/* � */
		{'a', 'e', 215},	/* � */
		{'A', '"', 216},	/* � */
		{'i', '`', 217},	/* � */
		{'O', '"', 218},	/* � */
		{'U', '"', 219},	/* � */
		{'E', '\'', 220},	/* � */
		{'i', '"', 221},	/* � */
		{'s', 's', 222},	/* � */
		{'O', '^', 223},	/* � */
		{'A', '\'', 224},	/* � */
		{'A', '~', 225},	/* � */
		{'a', '~', 226},	/* � */
		{'D', '-', 227},	/* � */
		{'d', '-', 228},	/* � */
		{'I', '\'', 229},	/* � */
		{'I', '`', 230},	/* � */
		{'O', '\'', 231},	/* � */
		{'O', '`', 232},	/* � */
		{'O', '~', 233},	/* � */
		{'o', '~', 234},	/* � */
		{'S', '~', 235},	/* � */
		{'s', '~', 236},	/* � */
		{'U', '\'', 237},	/* � */
		{'Y', '"', 238},	/* � */
		{'y', '"', 239},	/* � */
		{'p', '-', 240},	/* � */
		{'p', '~', 241},	/* � */
		{'~', '.', 242},	/* � */
		{'j', 'u', 243},	/* � */
		{'P', 'p', 244},	/* � */
		{'3', '4', 245},	/* � */
		{'-', '-', 246},	/* � */
		{'1', '4', 247},	/* � */
		{'1', '2', 248},	/* � */
		{'a', '_', 249},	/* � */
		{'o', '_', 250},	/* � */
		{'<', '<', 251},	/* � */
		{'x', 'x', 252},	/* � */
		{'>', '>', 253},	/* � */
		{'+', '-', 254},	/* � */
		{'n', 'u', 255},	/* (char excluded, is EOF on some systems */
		{NUL, NUL, NUL}
		};

#  else	/* _INCLUDE_HPUX_SOURCE */

char_u	digraphdefault[][3] = 		/* standard ISO digraphs */
	   {{'~', '!', 161},	/* � */
	    {'c', '|', 162},	/* � */
	    {'$', '$', 163},	/* � */
	    {'o', 'x', 164},	/* � */
	    {'Y', '-', 165},	/* � */
	    {'|', '|', 166},	/* � */
	    {'p', 'a', 167},	/* � */
	    {'"', '"', 168},	/* � */
	    {'c', 'O', 169},	/* � */
		{'a', '-', 170},	/* � */
		{'<', '<', 171},	/* � */
		{'-', ',', 172},	/* � */
		{'-', '-', 173},	/* � */
		{'r', 'O', 174},	/* � */
		{'-', '=', 175},	/* � */
		{'~', 'o', 176},	/* � */
		{'+', '-', 177},	/* � */
		{'2', '2', 178},	/* � */
		{'3', '3', 179},	/* � */
		{'\'', '\'', 180},	/* � */
		{'j', 'u', 181},	/* � */
		{'p', 'p', 182},	/* � */
		{'~', '.', 183},	/* � */
		{',', ',', 184},	/* � */
		{'1', '1', 185},	/* � */
		{'o', '-', 186},	/* � */
		{'>', '>', 187},	/* � */
		{'1', '4', 188},	/* � */
		{'1', '2', 189},	/* � */
		{'3', '4', 190},	/* � */
		{'~', '?', 191},	/* � */
		{'A', '`', 192},	/* � */
		{'A', '\'', 193},	/* � */
		{'A', '^', 194},	/* � */
		{'A', '~', 195},	/* � */
		{'A', '"', 196},	/* � */
		{'A', '@', 197},	/* � */
		{'A', 'E', 198},	/* � */
		{'C', ',', 199},	/* � */
		{'E', '`', 200},	/* � */
		{'E', '\'', 201},	/* � */
		{'E', '^', 202},	/* � */
		{'E', '"', 203},	/* � */
		{'I', '`', 204},	/* � */
		{'I', '\'', 205},	/* � */
		{'I', '^', 206},	/* � */
		{'I', '"', 207},	/* � */
		{'D', '-', 208},	/* � */
		{'N', '~', 209},	/* � */
		{'O', '`', 210},	/* � */
		{'O', '\'', 211},	/* � */
		{'O', '^', 212},	/* � */
		{'O', '~', 213},	/* � */
		{'O', '"', 214},	/* � */
		{'/', '\\', 215},	/* � */
		{'O', '/', 216},	/* � */
		{'U', '`', 217},	/* � */
		{'U', '\'', 218},	/* � */
		{'U', '^', 219},	/* � */
		{'U', '"', 220},	/* � */
		{'Y', '\'', 221},	/* � */
		{'I', 'p', 222},	/* � */
		{'s', 's', 223},	/* � */
		{'a', '`', 224},	/* � */
		{'a', '\'', 225},	/* � */
		{'a', '^', 226},	/* � */
		{'a', '~', 227},	/* � */
		{'a', '"', 228},	/* � */
		{'a', '@', 229},	/* � */
		{'a', 'e', 230},	/* � */
		{'c', ',', 231},	/* � */
		{'e', '`', 232},	/* � */
		{'e', '\'', 233},	/* � */
		{'e', '^', 234},	/* � */
		{'e', '"', 235},	/* � */
		{'i', '`', 236},	/* � */
		{'i', '\'', 237},	/* � */
		{'i', '^', 238},	/* � */
		{'i', '"', 239},	/* � */
		{'d', '-', 240},	/* � */
		{'n', '~', 241},	/* � */
		{'o', '`', 242},	/* � */
		{'o', '\'', 243},	/* � */
		{'o', '^', 244},	/* � */
		{'o', '~', 245},	/* � */
		{'o', '"', 246},	/* � */
		{':', '-', 247},	/* � */
		{'o', '/', 248},	/* � */
		{'u', '`', 249},	/* � */
		{'u', '\'', 250},	/* � */
		{'u', '^', 251},	/* � */
		{'u', '"', 252},	/* � */
		{'y', '\'', 253},	/* � */
		{'i', 'p', 254},	/* � */
		{'y', '"', 255},	/* (char excluded, is EOF on some systems */
		{NUL, NUL, NUL}
		};

#  endif	/* _INCLUDE_HPUX_SOURCE */
# endif	/* !MINT */
#endif	/* !MSDOS && !WIN32 */
 
/*
 * handle digraphs after typing a character
 */
	int
do_digraph(c)
	int		c;
{
	static int	backspaced;		/* character before K_BS */
	static int	lastchar;		/* last typed character */

	if (c == -1)				/* init values */
	{
		backspaced = -1;
	}
	else if (p_dg)
	{
		if (backspaced >= 0)
			c = getdigraph(backspaced, c, FALSE);
		backspaced = -1;
		if ((c == K_BS || c == Ctrl('H')) && lastchar >= 0)
			backspaced = lastchar;
	}
	lastchar = c;
	return c;
}

/*
 * lookup the pair char1, char2 in the digraph tables
 * if no match, return char2
 */
	static int
getexactdigraph(char1, char2, meta)
	int	char1;
	int	char2;
	int	meta;
{
	int		i;
	int		retval;

	if (IS_SPECIAL(char1) || IS_SPECIAL(char2))
		return char2;
	retval = 0;
	for (i = 0; ; ++i)			/* search added digraphs first */
	{
		if (i == digraphcount)	/* end of added table, search defaults */
		{
			for (i = 0; digraphdefault[i][0] != 0; ++i)
				if (digraphdefault[i][0] == char1 && digraphdefault[i][1] == char2)
				{
					retval = digraphdefault[i][2];
					break;
				}
			break;
		}
		if (digraphnew[i][0] == char1 && digraphnew[i][1] == char2)
		{
			retval = digraphnew[i][2];
			break;
		}
	}

	if (retval == 0)			/* digraph deleted or not found */
	{
		if (char1 == ' ' && meta)		/* <space> <char> --> meta-char */
			return (char2 | 0x80);
		return char2;
	}
	return retval;
}

/*
 * Get digraph.
 * Allow for both char1-char2 and char2-char1
 */
	int
getdigraph(char1, char2, meta)
	int	char1;
	int	char2;
	int	meta;
{
	int		retval;

	if (((retval = getexactdigraph(char1, char2, meta)) == char2) &&
														   (char1 != char2) &&
					((retval = getexactdigraph(char2, char1, meta)) == char1))
		return char2;
	return retval;
}

/*
 * put the digraphs in the argument string in the digraph table
 * format: {c1}{c2} char {c1}{c2} char ...
 */
	void
putdigraph(str)
	char_u *str;
{
	int		char1, char2, n;
	char_u	(*newtab)[3];
	int		i;

	while (*str)
	{
		str = skipwhite(str);
		if ((char1 = *str++) == 0 || (char2 = *str++) == 0)
			return;
		if (char1 == ESC || char2 == ESC)
		{
			EMSG("Escape not allowed in digraph");
			return;
		}
		str = skipwhite(str);
		if (!isdigit(*str))
		{
			emsg(e_number);
			return;
		}
		n = getdigits(&str);
		if (digraphnew)		/* search the table for existing entry */
		{
			for (i = 0; i < digraphcount; ++i)
				if (digraphnew[i][0] == char1 && digraphnew[i][1] == char2)
				{
					digraphnew[i][2] = n;
					break;
				}
			if (i < digraphcount)
				continue;
		}
		newtab = (char_u (*)[3])alloc(digraphcount * 3 + 3);
		if (newtab)
		{
			vim_memmove(newtab, digraphnew, (size_t)(digraphcount * 3));
			vim_free(digraphnew);
			digraphnew = newtab;
			digraphnew[digraphcount][0] = char1;
			digraphnew[digraphcount][1] = char2;
			digraphnew[digraphcount][2] = n;
			++digraphcount;
		}
	}
}

	void
listdigraphs()
{
	int		i;

	msg_outchar('\n');
	printdigraph(NULL);
	for (i = 0; digraphdefault[i][0] && !got_int; ++i)
	{
		if (getexactdigraph(digraphdefault[i][0], digraphdefault[i][1],
											   FALSE) == digraphdefault[i][2])
			printdigraph(digraphdefault[i]);
		mch_breakcheck();
	}
	for (i = 0; i < digraphcount && !got_int; ++i)
	{
		printdigraph(digraphnew[i]);
		mch_breakcheck();
	}
	must_redraw = CLEAR;	/* clear screen, because some digraphs may be wrong,
							 * in which case we messed up NextScreen */
}

	static void
printdigraph(p)
	char_u *p;
{
	char_u		buf[9];
	static int	len;

	if (p == NULL)
		len = 0;
	else if (p[2] != 0)
	{
		if (len > Columns - 11)
		{
			msg_outchar('\n');
			len = 0;
		}
		if (len)
			MSG_OUTSTR("   ");
		sprintf((char *)buf, "%c%c %c %3d", p[0], p[1], p[2], p[2]);
		msg_outstr(buf);
		len += 11;
	}
}

#endif /* DIGRAPHS */
