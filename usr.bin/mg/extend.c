/*	$OpenBSD$	*/

/*
 *	Extended (M-X) commands, rebinding, and	startup file processing.
 */

#include "def.h"
#include "kbd.h"

#ifndef NO_MACRO
#include "macro.h"
#endif /* !NO_MACRO */

#ifdef	FKEYS
#include "key.h"
#ifndef	NO_STARTUP
#ifndef	BINDKEY
#define	BINDKEY			/* bindkey is used by FKEYS startup code */
#endif /* !BINDKEY */
#endif /* !NO_STARTUP */
#endif /* FKEYS */

static int	 remap		__P((KEYMAP *, int, PF, KEYMAP *));
static KEYMAP	*realocmap	__P((KEYMAP *));
static VOID	 fixmap		__P((KEYMAP *, KEYMAP *, KEYMAP *));
static int	 dobind		__P((KEYMAP *, char *, int));
static char	*skipwhite	__P((char *));
static char	*parsetoken	__P((char *));
static int	 bindkey	__P((KEYMAP **, char *, KCHAR *, int));

/* 
 * Insert a string, mainly for use from macros (created by selfinsert) 
 */
/* ARGSUSED */
int
insert(f, n)
	int f, n;
{
	char	*cp;
	char	 buf[128];
#ifndef NO_MACRO
	int	 count, c;

	if (inmacro) {
		while (--n >= 0) {
			for (count = 0; count < maclcur->l_used; count++) {
				if ((((c = maclcur->l_text[count]) == '\n')
				    ? lnewline() : linsert(1, c)) != TRUE)
					return FALSE;
			}
		}
		maclcur = maclcur->l_fp;
		return TRUE;
	}
	if (n == 1)
		/* CFINS means selfinsert can tack on the end */
		thisflag |= CFINS;
#endif /* !NO_MACRO */
	if (eread("Insert: ", buf, sizeof(buf), EFNEW) == FALSE)
		return FALSE;
	while (--n >= 0) {
		cp = buf;
		while (*cp) {
			if (((*cp == '\n') ? lnewline() : linsert(1, *cp))
			    != TRUE)
				return FALSE;
			cp++;
		}
	}
	return TRUE;
}

/*
 * Bind a key to a function.  Cases range from the trivial (replacing an
 * existing binding) to the extremly complex (creating a new prefix in a
 * map_element that already has one, so the map_element must be split,
 * but the keymap doesn't have enough room for another map_element, so
 * the keymap is reallocated).	No attempt is made to reclaim space no
 * longer used, if this is a problem flags must be added to indicate
 * malloced verses static storage in both keymaps and map_elements.
 * Structure assignments would come in real handy, but K&R based compilers
 * don't have them.  Care is taken so running out of memory will leave
 * the keymap in a usable state.
 */
static int
remap(curmap, c, funct, pref_map)
	KEYMAP *curmap;		/* pointer to the map being changed */
	int     c;		/* character being changed */
	PF      funct;		/* function being changed to */
	KEYMAP *pref_map;	/* if funct==prefix, map to bind to or
				   NULL for new */
{
	int		 i, n1, n2, nold;
	KEYMAP		*mp;
	PF		*pfp;
	MAP_ELEMENT	*mep;

	if (ele >= &curmap->map_element[curmap->map_num] || c < ele->k_base) {
		if (ele > &curmap->map_element[0] && (funct != prefix ||
		    (ele - 1)->k_prefmap == NULL))
			n1 = c - (ele - 1)->k_num;
		else
			n1 = HUGE;
		if (ele < &curmap->map_element[curmap->map_num] &&
		    (funct != prefix || ele->k_prefmap == NULL))
			n2 = ele->k_base - c;
		else
			n2 = HUGE;
		if (n1 <= MAPELEDEF && n1 <= n2) {
			ele--;
			if ((pfp = (PF *)malloc((c - ele->k_base + 1) *
			    sizeof(PF))) == NULL) {
				ewprintf("Out of memory");
				return FALSE;
			}
			nold = ele->k_num - ele->k_base + 1;
			for (i = 0; i < nold; i++)
				pfp[i] = ele->k_funcp[i];
			while (--n1)
				pfp[i++] = curmap->map_default;
			pfp[i] = funct;
			ele->k_num = c;
			ele->k_funcp = pfp;
		} else if (n2 <= MAPELEDEF) {
			if ((pfp = (PF *)malloc((ele->k_num - c + 1) *
			    sizeof(PF))) == NULL) {
				ewprintf("Out of memory");
				return FALSE;
			}
			nold = ele->k_num - ele->k_base + 1;
			for (i = 0; i < nold; i++)
				pfp[i + n2] = ele->k_funcp[i];
			while (--n2)
				pfp[n2] = curmap->map_default;
			pfp[0] = funct;
			ele->k_base = c;
			ele->k_funcp = pfp;
		} else {
			if (curmap->map_num >= curmap->map_max &&
			    (curmap = realocmap(curmap)) == NULL)
				return FALSE;
			if ((pfp = (PF *)malloc(sizeof(PF))) == NULL) {
				ewprintf("Out of memory");
				return FALSE;
			}
			pfp[0] = funct;
			for (mep = &curmap->map_element[curmap->map_num];
			    mep > ele; mep--) {
				mep->k_base = (mep - 1)->k_base;
				mep->k_num = (mep - 1)->k_num;
				mep->k_funcp = (mep - 1)->k_funcp;
				mep->k_prefmap = (mep - 1)->k_prefmap;
			}
			ele->k_base = c;
			ele->k_num = c;
			ele->k_funcp = pfp;
			ele->k_prefmap = NULL;
			curmap->map_num++;
		}
		if (funct == prefix) {
			if (pref_map != NULL) {
				ele->k_prefmap = pref_map;
			} else {
				if (!(mp = (KEYMAP *)malloc(sizeof(KEYMAP) +
				    (MAPINIT - 1) * sizeof(MAP_ELEMENT)))) {
					ewprintf("Out of memory");
					ele->k_funcp[c - ele->k_base] =
					    curmap->map_default;
					return FALSE;
				}
				mp->map_num = 0;
				mp->map_max = MAPINIT;
				mp->map_default = rescan;
				ele->k_prefmap = mp;
			}
		}
	} else {
		n1 = c - ele->k_base;
		if (ele->k_funcp[n1] == funct && (funct != prefix ||
		    pref_map == NULL || pref_map == ele->k_prefmap))
			/* no change */
			return TRUE;
		if (funct != prefix || ele->k_prefmap == NULL) {
			if (ele->k_funcp[n1] == prefix)
				ele->k_prefmap = (KEYMAP *) NULL;
			/* easy case */
			ele->k_funcp[n1] = funct;
			if (funct == prefix) {
				if (pref_map != NULL)
					ele->k_prefmap = pref_map;
				else {
					if (!(mp = malloc(sizeof(KEYMAP) +
					    (MAPINIT - 1) * 
					    sizeof(MAP_ELEMENT)))) {
						ewprintf("Out of memory");
						ele->k_funcp[c - ele->k_base] =
						    curmap->map_default;
						return FALSE;
					}
					mp->map_num = 0;
					mp->map_max = MAPINIT;
					mp->map_default = rescan;
					ele->k_prefmap = mp;
				}
			}
		} else {
			/*
			 * This case is the splits.
			 * Determine which side of the break c goes on
			 * 0 = after break; 1 = before break
			 */
			n2 = 1;
			for (i = 0; n2 && i < n1; i++)
				n2 &= ele->k_funcp[i] != prefix;
			if (curmap->map_num >= curmap->map_max &&
			    (curmap = realocmap(curmap)) == NULL)
				return FALSE;
			if ((pfp = malloc((ele->k_num - c + !n2) * 
			    sizeof(PF))) == NULL) {
				ewprintf("Out of memory");
				return FALSE;
			}
			ele->k_funcp[n1] = prefix;
			for (i = n1 + n2; i <= ele->k_num - ele->k_base; i++)
				pfp[i - n1 - n2] = ele->k_funcp[i];
			for (mep = &curmap->map_element[curmap->map_num];
			    mep > ele; mep--) {
				mep->k_base = (mep - 1)->k_base;
				mep->k_num = (mep - 1)->k_num;
				mep->k_funcp = (mep - 1)->k_funcp;
				mep->k_prefmap = (mep - 1)->k_prefmap;
			}
			ele->k_num = c - !n2;
			(ele + 1)->k_base = c + n2;
			(ele + 1)->k_funcp = pfp;
			ele += !n2;
			ele->k_prefmap = NULL;
			curmap->map_num++;
			if (pref_map == NULL) {
				if ((mp = malloc(sizeof(KEYMAP) + (MAPINIT - 1)
				    * sizeof(MAP_ELEMENT))) == NULL) {
					ewprintf("Out of memory");
					ele->k_funcp[c - ele->k_base] =
					    curmap->map_default;
					return FALSE;
				}
				mp->map_num = 0;
				mp->map_max = MAPINIT;
				mp->map_default = rescan;
				ele->k_prefmap = mp;
			} else
				ele->k_prefmap = pref_map;
		}
	}
	return TRUE;
}

/*
 * Reallocate a keymap, used above.
 */
static KEYMAP *
realocmap(curmap)
	KEYMAP *curmap;
{
	KEYMAP	*mp;
	int	 i;

	if ((mp = (KEYMAP *)malloc((unsigned)(sizeof(KEYMAP) +
	    (curmap->map_max + (MAPGROW - 1)) * 
	    sizeof(MAP_ELEMENT)))) == NULL) {
		ewprintf("Out of memory");
		return NULL;
	}
	mp->map_num = curmap->map_num;
	mp->map_max = curmap->map_max + MAPGROW;
	mp->map_default = curmap->map_default;
	for (i = curmap->map_num; i--;) {
		mp->map_element[i].k_base = curmap->map_element[i].k_base;
		mp->map_element[i].k_num = curmap->map_element[i].k_num;
		mp->map_element[i].k_funcp = curmap->map_element[i].k_funcp;
		mp->map_element[i].k_prefmap = curmap->map_element[i].k_prefmap;
	}
	for (i = nmaps; i--;) {
		if (map_table[i].p_map == curmap)
			map_table[i].p_map = mp;
		else
			fixmap(curmap, mp, map_table[i].p_map);
	}
	ele = &mp->map_element[ele - &curmap->map_element[0]];
	return mp;
}

/*
 * Fix references to a reallocated keymap (recursive).
 */
static VOID
fixmap(curmap, mp, mt)
	KEYMAP *mt;
	KEYMAP *curmap;
	KEYMAP *mp;
{
	int	 i;

	for (i = mt->map_num; i--;) {
		if (mt->map_element[i].k_prefmap != NULL) {
			if (mt->map_element[i].k_prefmap == curmap)
				mt->map_element[i].k_prefmap = mp;
			else
				fixmap(curmap, mp, mt->map_element[i].k_prefmap);
		}
	}
}

/*
 * do the input for local-set-key, global-set-key  and define-key
 * then call remap to do the work.
 */
static int
dobind(curmap, p, unbind)
	KEYMAP *curmap;
	char   *p;
	int     unbind;
{
	KEYMAP	*pref_map = NULL;
	PF	 funct;
	char	 prompt[80];
	char	*pep;
	int	 c, s;

#ifndef NO_MACRO
	if (macrodef) {
		/*
		 * Keystrokes aren't collected. Not hard, but pretty useless.
		 * Would not work for function keys in any case.
		 */
		ewprintf("Can't rebind key in macro");
		return FALSE;
	}
#ifndef NO_STARTUP
	if (inmacro) {
		for (s = 0; s < maclcur->l_used - 1; s++) {
			if (doscan(curmap, c = CHARMASK(maclcur->l_text[s]))
			    != prefix) {
				if (remap(curmap, c, prefix, (KEYMAP *)NULL)
				    != TRUE)
					return FALSE;
			}
			curmap = ele->k_prefmap;
		}
		(VOID)doscan(curmap, c = maclcur->l_text[s]);
		maclcur = maclcur->l_fp;
	} else {
#endif /* !NO_STARTUP */
#endif /* !NO_MACRO */
		(VOID)strcpy(prompt, p);
		pep = prompt + strlen(prompt);
		for (;;) {
			ewprintf("%s", prompt);
			pep[-1] = ' ';
			pep = keyname(pep, c = getkey(FALSE));
			if (doscan(curmap, c) != prefix)
				break;
			*pep++ = '-';
			*pep = '\0';
			curmap = ele->k_prefmap;
		}
#ifndef NO_STARTUP
	}
#endif /* !NO_STARTUP */
	if (unbind)
		funct = rescan;
	else {
		if ((s = eread("%s to command: ", prompt, 80, EFFUNC | EFNEW,
		    prompt)) != TRUE)
			return s;
		if (((funct = name_function(prompt)) == prefix) ?
		    (pref_map = name_map(prompt)) == NULL : funct == NULL) {
			ewprintf("[No match]");
			return FALSE;
		}
	}
	return remap(curmap, c, funct, pref_map);
}

/*
 * bindkey: bind key sequence to a function in the specified map.  Used by 
 * excline so it can bind function keys.  To close to release to change 
 * calling sequence, should just pass KEYMAP *curmap rather than 
 * KEYMAP **mapp.
 */
#ifdef	BINDKEY
static int
bindkey(mapp, fname, keys, kcount)
	KEYMAP **mapp;
	char    *fname;
	KCHAR   *keys;
	int      kcount;
{
	KEYMAP	*curmap = *mapp;
	KEYMAP	*pref_map = NULL;
	PF	 funct;
	int	 c;

	if (fname == NULL)
		funct = rescan;
	else if (((funct = name_function(fname)) == prefix) ?
	    (pref_map = name_map(fname)) == NULL : funct == NULL) {
		ewprintf("[No match: %s]", fname);
		return FALSE;
	}
	while (--kcount) {
		if (doscan(curmap, c = *keys++) != prefix) {
			if (remap(curmap, c, prefix, (KEYMAP *)NULL) != TRUE)
				return FALSE;
		}
		curmap = ele->k_prefmap;
	}
	(VOID)doscan(curmap, c = *keys);
	return remap(curmap, c, funct, pref_map);
}

#ifdef FKEYS
/*
 * Wrapper for bindkey() that converts escapes.
 */
int
dobindkey(map, func, str)
	KEYMAP *map;
	char   *func;
	char   *str;
{
	int	 i;

	for (i = 0; *str && i < MAXKEY; i++) {
		/* XXX - convert numbers w/ strol()? */
		if (*str != '\\')
			key.k_chars[i] = *str;
		else {
			switch (*++str) {
			case 't':
			case 'T':
				key.k_chars[i] = '\t';
				break;
			case 'n':
			case 'N':
				key.k_chars[i] = '\n';
				break;
			case 'r':
			case 'R':
				key.k_chars[i] = '\r';
				break;
			case 'e':
			case 'E':
				key.k_chars[i] = CCHR('[');
				break;
			}
		}
		str++;
	}
	key.k_count = i;
	return (bindkey(&map, func, key.k_chars, key.k_count));
}
#endif /* FKEYS */
#endif /* BINDKEY */

/*
 * This function modifies the fundamental keyboard map.
 */
/* ARGSUSED */
int
bindtokey(f, n)
	int f, n;
{
	return dobind(map_table[0].p_map, "Global set key: ", FALSE);
}

/*
 * This function modifies the current mode's keyboard map.
 */
/* ARGSUSED */
int
localbind(f, n)
	int f, n;
{
	return dobind(curbp->b_modes[curbp->b_nmodes]->p_map, 
	    "Local set key: ", FALSE);
}

/*
 * This function redefines a key in any keymap.
 */
/* ARGSUSED */
int
define_key(f, n)
	int f, n;
{
	static char	 buf[48] = "Define key map: ";
	MAPS		*mp;

	buf[16] = '\0';
	if (eread(buf, &buf[16], 48 - 16, EFNEW) != TRUE)
		return FALSE;
	if ((mp = name_mode(&buf[16])) == NULL) {
		ewprintf("Unknown map %s", &buf[16]);
		return FALSE;
	}
	(VOID)strncat(&buf[16], " key: ", 48 - 16 - 1);
	return dobind(mp->p_map, buf, FALSE);
}

int
unbindtokey(f, n)
	int f, n;
{
	return dobind(map_table[0].p_map, "Global unset key: ", TRUE);
}

int
localunbind(f, n)
	int f, n;
{
	return dobind(curbp->b_modes[curbp->b_nmodes]->p_map,
	    "Local unset key: ", TRUE);
}

/*
 * Extended command. Call the message line routine to read in the command 
 * name and apply autocompletion to it. When it comes back, look the name 
 * up in the symbol table and run the command if it is found.  Print an 
 * error if there is anything wrong.
 */
int
extend(f, n)
	int f, n;
{
	PF	 funct;
	int	 s;
	char	 xname[NXNAME];

	if (!(f & FFARG))
		s = eread("M-x ", xname, NXNAME, EFNEW | EFFUNC);
	else
		s = eread("%d M-x ", xname, NXNAME, EFNEW | EFFUNC, n);
	if (s != TRUE)
		return s;
	if ((funct = name_function(xname)) != NULL) {
#ifndef NO_MACRO
		if (macrodef) {
			LINE	*lp = maclcur;
			macro[macrocount - 1].m_funct = funct;
			maclcur = lp->l_bp;
			maclcur->l_fp = lp->l_fp;
			free((char *)lp);
		}
#endif /* !NO_MACRO */
		return (*funct)(f, n);
	}
	ewprintf("[No match]");
	return FALSE;
}

#ifndef NO_STARTUP
/*
 * Define the commands needed to do startup-file processing.
 * This code is mostly a kludge just so we can get startup-file processing.
 *
 * If you're serious about having this code, you should rewrite it.
 * To wit:
 *	It has lots of funny things in it to make the startup-file look
 *	like a GNU startup file; mostly dealing with parens and semicolons.
 *	This should all vanish.
 *
 * We define eval-expression because it's easy.	 It can make
 * *-set-key or define-key set an arbitrary key sequence, so it isn't
 * useless.
 */

/*
 * evalexpr - get one line from the user, and run it.
 */
/* ARGSUSED */
int
evalexpr(f, n)
	int f, n;
{
	int	 s;
	char	 exbuf[128];

	if ((s = ereply("Eval: ", exbuf, 128)) != TRUE)
		return s;
	return excline(exbuf);
}

/*
 * evalbuffer - evaluate the current buffer as line commands. Useful for 
 * testing startup files.
 */
/* ARGSUSED */
int
evalbuffer(f, n)
	int f, n;
{
	LINE		*lp;
	BUFFER		*bp = curbp;
	int		 s;
	static char	 excbuf[128];

	for (lp = lforw(bp->b_linep); lp != bp->b_linep; lp = lforw(lp)) {
		if (llength(lp) >= 128)
			return FALSE;
		(VOID)strncpy(excbuf, ltext(lp), llength(lp));

		/* make sure it's terminated */
		excbuf[llength(lp)] = '\0';
		if ((s = excline(excbuf)) != TRUE)
			return s;
	}
	return TRUE;
}

/*
 * evalfile - go get a file and evaluate it as line commands. You can
 *	go get your own startup file if need be.
 */
/* ARGSUSED */
int
evalfile(f, n)
	int f, n;
{
	int	 s;
	char	 fname[NFILEN];

	if ((s = ereply("Load file: ", fname, NFILEN)) != TRUE)
		return s;
	return load(fname);
}

/*
 * load - go load the file name we got passed.
 */
int
load(fname)
	char *fname;
{
	int	 s = TRUE;
	int	 nbytes;
	char	 excbuf[128];

	if ((fname = adjustname(fname)) == NULL)
		/* just to be careful */
		return FALSE;

	if (ffropen(fname, (BUFFER *)NULL) != FIOSUC)
		return FALSE;

	while ((s = ffgetline(excbuf, sizeof(excbuf) - 1, &nbytes)) == FIOSUC) {
		excbuf[nbytes] = '\0';
		if (excline(excbuf) != TRUE) {
			s = FIOERR;
			ewprintf("Error loading file %s", fname);
			break;
		}
	}
	(VOID)ffclose((BUFFER *)NULL);
	excbuf[nbytes] = '\0';
	if (s != FIOEOF || (nbytes && excline(excbuf) != TRUE))
		return FALSE;
	return TRUE;
}

/*
 * excline - run a line from a load file or eval-expression.  if FKEYS is 
 * defined, duplicate functionallity of dobind so function key values don't 
 * have to fit in type char.
 */
int
excline(line)
	char *line;
{
	PF	 fp;
	LINE	*lp, *np;
	int	 status, c, f, n;
	char	*funcp;
	char	*argp = NULL;
#ifdef	FKEYS
	int	 bind;
	KEYMAP	*curmap;
	MAPS	*mp;
#define BINDARG		0  /* this arg is key to bind (local/global set key) */
#define	BINDNO		1  /* not binding or non-quoted BINDARG */
#define BINDNEXT	2  /* next arg " (define-key) */
#define BINDDO		3  /* already found key to bind */
#define BINDEXT		1  /* space for trailing \0 */
#else /* FKEYS */
#define BINDEXT		0
#endif /* FKEYS */

	lp = NULL;

	if (macrodef || inmacro) {
		ewprintf("Not now!");
		return FALSE;
	}
	f = 0;
	n = 1;
	funcp = skipwhite(line);
	if (*funcp == '\0')
		return TRUE;	/* No error on blank lines */
	line = parsetoken(funcp);
	if (*line != '\0') {
		*line++ = '\0';
		line = skipwhite(line);
		if ((*line >= '0' && *line <= '9') || *line == '-') {
			argp = line;
			line = parsetoken(line);
		}
	}
	if (argp != NULL) {
		f = FFARG;
		n = atoi(argp);
	}
	if ((fp = name_function(funcp)) == NULL) {
		ewprintf("Unknown function: %s", funcp);
		return FALSE;
	}
#ifdef	FKEYS
	if (fp == bindtokey || fp == unbindtokey) {
		bind = BINDARG;
		curmap = map_table[0].p_map;
	} else if (fp == localbind || fp == localunbind) {
		bind = BINDARG;
		curmap = curbp->b_modes[curbp->b_nmodes]->p_map;
	} else if (fp == define_key)
		bind = BINDNEXT;
	else
		bind = BINDNO;
#endif /* FKEYS */
	/* Pack away all the args now... */
	if ((np = lalloc(0)) == FALSE)
		return FALSE;
	np->l_fp = np->l_bp = maclcur = np;
	while (*line != '\0') {
		argp = skipwhite(line);
		if (*argp == '\0')
			break;
		line = parsetoken(argp);
		if (*argp != '"') {
			if (*argp == '\'')
				++argp;
			if (!(lp = lalloc((int) (line - argp) + BINDEXT))) {
				status = FALSE;
				goto cleanup;
			}
			bcopy(argp, ltext(lp), (int)(line - argp));
#ifdef	FKEYS
			/* don't count BINDEXT */
			lp->l_used--;
			if (bind == BINDARG)
				bind = BINDNO;
#endif /* FKEYS */
		} else {
			/* quoted strings are special */
			++argp;
#ifdef	FKEYS
			if (bind != BINDARG) {
#endif /* FKEYS */
				lp = lalloc((int)(line - argp) + BINDEXT);
				if (lp == NULL) {
					status = FALSE;
					goto cleanup;
				}
				lp->l_used = 0;
#ifdef	FKEYS
			} else {
				key.k_count = 0;
			}
#endif /* FKEYS */
			while (*argp != '"' && *argp != '\0') {
				if (*argp != '\\')
					c = *argp++;
				else {
					switch (*++argp) {
					case 't':
					case 'T':
						c = CCHR('I');
						break;
					case 'n':
					case 'N':
						c = CCHR('J');
						break;
					case 'r':
					case 'R':
						c = CCHR('M');
						break;
					case 'e':
					case 'E':
						c = CCHR('[');
						break;
					case '^':
						/*
						 * split into two statements
						 * due to bug in OSK cpp
						 */
						c = CHARMASK(*++argp);
						c = ISLOWER(c) ?
						    CCHR(TOUPPER(c)) : CCHR(c);
						break;
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
						c = *argp - '0';
						if (argp[1] <= '7' && 
						    argp[1] >= '0') {
							c <<= 3;
							c += *++argp - '0';
							if (argp[1] <= '7' &&
							    argp[1] >= '0') {
								c <<= 3;
								c += *++argp
								    - '0';
							}
						}
						break;
#ifdef	FKEYS
					case 'f':
					case 'F':
						c = *++argp - '0';
						if (ISDIGIT(argp[1])) {
							c *= 10;
							c += *++argp - '0';
						}
						c += KFIRST;
						break;
#endif /* FKEYS */
					default:
						c = CHARMASK(*argp);
						break;
					}
					argp++;
				}
#ifdef	FKEYS
				if (bind == BINDARG)
					key.k_chars[key.k_count++] = c;
				else
#endif /* FKEYS */
					lp->l_text[lp->l_used++] = c;
			}
			if (*line)
				line++;
		}
#ifdef	FKEYS
		switch (bind) {
		case BINDARG:
			bind = BINDDO;
			break;
		case BINDNEXT:
			lp->l_text[lp->l_used] = '\0';
			if ((mp = name_mode(lp->l_text)) == NULL) {
				ewprintf("No such mode: %s", lp->l_text);
				status = FALSE;
				free((char *)lp);
				goto cleanup;
			}
			curmap = mp->p_map;
			free((char *)lp);
			bind = BINDARG;
			break;
		default:
#endif /* FKEYS */
			lp->l_fp = np->l_fp;
			lp->l_bp = np;
			np->l_fp = lp;
			np = lp;
#ifdef	FKEYS
		}
#endif /* FKEYS */
	}
#ifdef	FKEYS
	switch (bind) {
	default:
		ewprintf("Bad args to set key");
		status = FALSE;
		break;
	case BINDDO:
		if (fp != unbindtokey && fp != localunbind) {
			lp->l_text[lp->l_used] = '\0';
			status = bindkey(&curmap, lp->l_text, key.k_chars,
			    key.k_count);
		} else
			status = bindkey(&curmap, (char *)NULL, key.k_chars,
			    key.k_count);
		break;
	case BINDNO:
#endif /* FKEYS */
		inmacro = TRUE;
		maclcur = maclcur->l_fp;
		status = (*fp)(f, n);
		inmacro = FALSE;
#ifdef	FKEYS
	}
#endif /* FKEYS */
cleanup:
	lp = maclcur->l_fp;
	while (lp != maclcur) {
		np = lp->l_fp;
		free((char *)lp);
		lp = np;
	}
	free((char *)lp);
	return status;
}

/*
 * a pair of utility functions for the above
 */
static char *
skipwhite(s)
	char *s;
{
	while (*s == ' ' || *s == '\t' || *s == ')' || *s == '(')
		s++;
	if (*s == ';')
		*s = '\0';
	return s;
}

static char *
parsetoken(s)
	char  *s;
{
	if (*s != '"') {
		while (*s && *s != ' ' && *s != '\t' && *s != ')' && *s != '(')
			s++;
		if (*s == ';')
			*s = '\0';
	} else
		do {
			/* 
			 * Strings get special treatment.  
			 * Beware: You can \ out the end of the string! 
			 */
			if (*s == '\\')
				++s;
		} while (*++s != '"' && *s != '\0');
	return s;
}
#endif /* !NO_STARTUP */
