/*	$OpenBSD: help.c,v 1.14 2001/05/24 09:47:34 art Exp $	*/

/*
 * Help functions for Mg 2
 */

#include "def.h"
#include "funmap.h"

#ifndef NO_HELP
#include "kbd.h"
#include "key.h"
#ifndef NO_MACRO
#include "macro.h"
#endif /* !NO_MACRO */

static int	showall(BUFFER *, KEYMAP *, char *);
static int	findbind(KEYMAP *, PF, char *, size_t);

/*
 * Read a key from the keyboard, and look it up in the keymap.
 * Display the name of the function currently bound to the key.
 */
/* ARGSUSED */
int
desckey(f, n)
	int f, n;
{
	KEYMAP	*curmap;
	PF	 funct;
	int	 c, m, i;
	char	*pep;
	char	 prompt[80];

#ifndef NO_MACRO
	if (inmacro)
		return TRUE;	/* ignore inside keyboard macro */
#endif /* !NO_MACRO */
	pep = prompt + strlcpy(prompt, "Describe key briefly: ", sizeof(prompt));
	key.k_count = 0;
	m = curbp->b_nmodes;
	curmap = curbp->b_modes[m]->p_map;
	for (;;) {
		for (;;) {
			ewprintf("%s", prompt);
			pep[-1] = ' ';
			pep = keyname(pep, sizeof(prompt) - (pep - prompt),
			    key.k_chars[key.k_count++] = c = getkey(FALSE));
			if ((funct = doscan(curmap, c, &curmap)) != NULL)
				break;
			*pep++ = '-';
			*pep = '\0';
		}
		if (funct != rescan)
			break;
		if (ISUPPER(key.k_chars[key.k_count - 1])) {
			funct = doscan(curmap,
			    TOLOWER(key.k_chars[key.k_count - 1]), &curmap);
			if (funct == NULL) {
				*pep++ = '-';
				*pep = '\0';
				continue;
			}
			if (funct != rescan)
				break;
		}
nextmode:
		if (--m < 0)
			break;
		curmap = curbp->b_modes[m]->p_map;
		for (i = 0; i < key.k_count; i++) {
			funct = doscan(curmap, key.k_chars[i], &curmap);
			if (funct != NULL) {
				if (i == key.k_count - 1 && funct != rescan)
					goto found;
				funct = rescan;
				goto nextmode;
			}
		}
		*pep++ = '-';
		*pep = '\0';
	}
found:
	if (funct == rescan || funct == selfinsert)
		ewprintf("%k is not bound to any function");
	else if ((pep = function_name(funct)) != NULL)
		ewprintf("%k runs the command %s", pep);
	else
		ewprintf("%k is bound to an unnamed function");
	return TRUE;
}

/*
 * This function creates a table, listing all of the command
 * keys and their current bindings, and stores the table in the
 * *help* pop-up buffer.  This lets Mg produce it's own wall chart.
 */
/* ARGSUSED */
int
wallchart(f, n)
	int f, n;
{
	int		 m;
	BUFFER		*bp;

	bp = bfind("*help*", TRUE);
	if (bclear(bp) != TRUE)
		/* clear it out */
		return FALSE;
	for (m = curbp->b_nmodes; m > 0; m--) {
		if ((addlinef(bp, "Local keybindings for mode %s:",
				curbp->b_modes[m]->p_name) == FALSE) ||
		    (showall(bp, curbp->b_modes[m]->p_map, "") == FALSE) ||
		    (addline(bp, "") == FALSE))
			return FALSE;
	}
	if ((addline(bp, "Global bindings:") == FALSE) ||
	    (showall(bp, fundamental_map, "") == FALSE))
		return FALSE;
	return popbuftop(bp);
}

static int
showall(BUFFER *bp, KEYMAP *map, char *prefix)
{
	KEYMAP *newmap;
	char buf[80], key[16];
	PF fun;
	int c;

	if (addline(bp, "") == FALSE)
		return FALSE;

	/* XXX - 256 ? */
	for (c = 0; c < 256; c++) {
		fun = doscan(map, c, &newmap);
		if (fun == rescan || fun == selfinsert)
			continue;
		keyname(buf, sizeof(buf), c);
		snprintf(key, sizeof key, "%s%s ", prefix, buf);
		if (fun == NULL) {
			if (showall(bp, newmap, key) == FALSE)
				return FALSE;
		} else {
			if (addlinef(bp, "%-16s%s", key,
				    function_name(fun)) == FALSE)
				return FALSE;
		}
	}

	return TRUE;
}

int
help_help(f, n)
	int f, n;
{
	KEYMAP	*kp;
	PF	 funct;

	if ((kp = name_map("help")) == NULL)
		return FALSE;
	ewprintf("a b c: ");
	do {
		funct = doscan(kp, getkey(FALSE), NULL);
	} while (funct == NULL || funct == help_help);
#ifndef NO_MACRO
	if (macrodef && macrocount < MAXMACRO)
		macro[macrocount - 1].m_funct = funct;
#endif /* !NO_MACRO */
	return (*funct)(f, n);
}

/* ARGSUSED */
int
apropos_command(f, n)
	int f, n;
{
	BUFFER		*bp;
	LIST		*fnames, *el;
	char		 string[32];

	if (eread("apropos: ", string, sizeof(string), EFNEW) == ABORT)
		return ABORT;
	/* FALSE means we got a 0 character string, which is fine */
	bp = bfind("*help*", TRUE);
	if (bclear(bp) == FALSE)
		return FALSE;

	fnames = complete_function_list("", NULL);
	for (el = fnames; el != NULL; el = el->l_next) {
		char buf[32];

		if (strstr(el->l_name, string) == NULL)
			continue;

		buf[0] = '\0';
		findbind(fundamental_map, name_function(el->l_name),
		    buf, sizeof(buf));

		if (addlinef(bp, "%-32s%s", el->l_name,  buf) == FALSE) {
			free_file_list(fnames);
			return FALSE;
		}
	}
	free_file_list(fnames);
	return popbuftop(bp);
}

static int
findbind(KEYMAP *map, PF fun, char *buf, size_t len)
{
	KEYMAP *newmap;
	PF nfun;
	char buf2[16], key[16];
	int c;

	/* XXX - 256 ? */
	for (c = 0; c < 256; c++) {
		nfun = doscan(map, c, &newmap);
		if (nfun == fun) {
			keyname(buf, len, c);
			return TRUE;
		}
		if (nfun == NULL) {
			if (findbind(newmap, fun, buf2, sizeof(buf2)) == TRUE) {
				keyname(key, sizeof(key), c);
				snprintf(buf, len, "%s %s", key, buf2);
				return TRUE;
			}
		}
	}

	return FALSE;
}
#endif /* !NO_HELP */
