/*
 * Commands to toggle modes.   Without an argument, these functions will 
 * toggle the given mode.  A negative or zero argument will turn the mode 
 * off.  A positive argument will turn the mode on.
 */

#include "def.h"
#include "kbd.h"

static int	changemode	__P((int, int, char *));

int	 defb_nmodes = 0;
MAPS	*defb_modes[PBMODES] = { &map_table[0] };
int	 defb_flag = 0;

static int
changemode(f, n, mode)
	int   f, n;
	char *mode;
{
	int	 i;
	MAPS	*m;

	if ((m = name_mode(mode)) == NULL) {
		ewprintf("Can't find mode %s", mode);
		return FALSE;
	}
	if (!(f & FFARG)) {
		for (i = 0; i <= curbp->b_nmodes; i++)
			if (curbp->b_modes[i] == m) {
				/* mode already set */
				n = 0;
				break;
			}
	}
	if (n > 0) {
		for (i = 0; i <= curbp->b_nmodes; i++)
			if (curbp->b_modes[i] == m)
				/* mode already set */
				return TRUE;
		if (curbp->b_nmodes >= PBMODES - 1) {
			ewprintf("Too many modes");
			return FALSE;
		}
		curbp->b_modes[++(curbp->b_nmodes)] = m;
	} else {
		/* fundamental is b_modes[0] and can't be unset */
		for (i = 1; i <= curbp->b_nmodes && m != curbp->b_modes[i]; 
		    i++);
		if (i > curbp->b_nmodes)
			return TRUE;	/* mode wasn't set */
		for (; i < curbp->b_nmodes; i++)
			curbp->b_modes[i] = curbp->b_modes[i + 1];
		curbp->b_nmodes--;
	}
	upmodes(curbp);
	return TRUE;
}

int
indentmode(f, n)
	int f, n;
{
	return changemode(f, n, "indent");
}

int
fillmode(f, n)
	int f, n;
{
	return changemode(f, n, "fill");
}

/*
 * Fake the GNU "blink-matching-paren" variable.
 */
int
blinkparen(f, n)
	int f, n;
{
	return changemode(f, n, "blink");
}

#ifdef NOTAB
int
notabmode(f, n)
	int f, n;
{
	if (changemode(f, n, "notab") == FALSE)
		return FALSE;
	if (f & FFARG) {
		if (n <= 0)
			curbp->b_flag &= ~BFNOTAB;
		else
			curbp->b_flag |= BFNOTAB;
	} else
		curbp->b_flag ^= BFNOTAB;
	return TRUE;
}
#endif	/* NOTAB */

int
overwrite(f, n)
	int f, n;
{
	if (changemode(f, n, "overwrite") == FALSE)
		return FALSE;
	if (f & FFARG) {
		if (n <= 0)
			curbp->b_flag &= ~BFOVERWRITE;
		else
			curbp->b_flag |= BFOVERWRITE;
	} else
		curbp->b_flag ^= BFOVERWRITE;
	return TRUE;
}

int
set_default_mode(f, n)
	int f, n;
{
	int	 i;
	MAPS	*m;
	char	 mode[32];

	if (eread("Set Default Mode: ", mode, 32, EFNEW) != TRUE)
		return ABORT;
	if ((m = name_mode(mode)) == NULL) {
		ewprintf("can't find mode %s", mode);
		return FALSE;
	}
	if (!(f & FFARG)) {
		for (i = 0; i <= defb_nmodes; i++)
			if (defb_modes[i] == m) {
				/* mode already set */
				n = 0;
				break;
			}
	}
	if (n > 0) {
		for (i = 0; i <= defb_nmodes; i++)
			if (defb_modes[i] == m)
				/* mode already set */
				return TRUE;
		if (defb_nmodes >= PBMODES - 1) {
			ewprintf("Too many modes");
			return FALSE;
		}
		defb_modes[++defb_nmodes] = m;
	} else {
		/* fundamental is defb_modes[0] and can't be unset */
		for (i = 1; i <= defb_nmodes && m != defb_modes[i]; i++);
		if (i > defb_nmodes)
			/* mode was not set */
			return TRUE;
		for (; i < defb_nmodes; i++)
			defb_modes[i] = defb_modes[i + 1];
		defb_nmodes--;
	}
	if (strcmp(mode, "overwrite") == 0) {
		if (n <= 0) 
			defb_flag &= ~BFOVERWRITE;
		else 
			defb_flag |= BFOVERWRITE;
	}
#ifdef NOTAB
	if (strcmp(mode, "notab") == 0)
		if (n <= 0)
			defb_flag &= ~BFNOTAB;
		else
			defb_flag |= BFNOTAB;
#endif	/* NOTAB */
	return TRUE;
}
