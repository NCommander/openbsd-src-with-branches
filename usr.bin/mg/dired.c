/*	$OpenBSD: dired.c,v 1.10 2002/03/11 13:02:56 vincent Exp $	*/

/* dired module for mg 2a	 */
/* by Robert A. Larson		 */

#include "def.h"
#include "kbd.h"

#ifndef NO_DIRED

int d_findfile(int, int);

static PF dired_pf[] = {
	d_findfile,
};

static struct KEYMAPE (1 + IMAPEXT) diredmap = {
	1,
	1 + IMAPEXT,
	rescan,
	{
		{ CCHR('M'), CCHR('M'), dired_pf, NULL },
	}
};

/* ARGSUSED */
int
dired(int f, int n)
{
	static int inited = 0;
	char	dirname[NFILEN];
	BUFFER *bp;

	if (inited == 0) {
		maps_add((KEYMAP *)&diredmap, "dired");
		inited = 1;
	}

	dirname[0] = '\0';
	if (eread("Dired: ", dirname, NFILEN, EFNEW | EFCR) == ABORT)
		return ABORT;
	if ((bp = dired_(dirname)) == NULL)
		return FALSE;
	bp->b_modes[0] = name_mode("fundamental");
	bp->b_modes[1] = name_mode("dired");
	bp->b_nmodes = 1;
	curbp = bp;
	return showbuffer(bp, curwp, WFHARD | WFMODE);
}

/* ARGSUSED */
int
d_otherwindow(int f, int n)
{
	char	dirname[NFILEN];
	BUFFER	*bp;
	MGWIN	*wp;

	dirname[0] = '\0';
	if (eread("Dired other window: ", dirname, NFILEN, EFNEW | EFCR) == ABORT)
		return ABORT;
	if ((bp = dired_(dirname)) == NULL)
		return FALSE;
	if ((wp = popbuf(bp)) == NULL)
		return FALSE;
	curbp = bp;
	curwp = wp;
	return TRUE;
}

/* ARGSUSED */
int
d_del(int f, int n)
{
	if (n < 0)
		return FALSE;
	while (n--) {
		if (llength(curwp->w_dotp) > 0)
			lputc(curwp->w_dotp, 0, 'D');
		if (lforw(curwp->w_dotp) != curbp->b_linep)
			curwp->w_dotp = lforw(curwp->w_dotp);
	}
	curwp->w_flag |= WFEDIT | WFMOVE;
	curwp->w_doto = 0;
	return TRUE;
}

/* ARGSUSED */
int
d_undel(int f, int n)
{
	if (n < 0)
		return d_undelbak(f, -n);
	while (n--) {
		if (llength(curwp->w_dotp) > 0)
			lputc(curwp->w_dotp, 0, ' ');
		if (lforw(curwp->w_dotp) != curbp->b_linep)
			curwp->w_dotp = lforw(curwp->w_dotp);
	}
	curwp->w_flag |= WFEDIT | WFMOVE;
	curwp->w_doto = 0;
	return TRUE;
}

/* ARGSUSED */
int
d_undelbak(int f, int n)
{
	if (n < 0)
		return d_undel(f, -n);
	while (n--) {
		if (llength(curwp->w_dotp) > 0)
			lputc(curwp->w_dotp, 0, ' ');
		if (lback(curwp->w_dotp) != curbp->b_linep)
			curwp->w_dotp = lback(curwp->w_dotp);
	}
	curwp->w_doto = 0;
	curwp->w_flag |= WFEDIT | WFMOVE;
	return TRUE;
}

/* ARGSUSED */
int
d_findfile(int f, int n)
{
	BUFFER *bp;
	int	s;
	char	fname[NFILEN];

	if ((s = d_makename(curwp->w_dotp, fname, sizeof fname)) == ABORT)
		return FALSE;
	if ((bp = (s ? dired_(fname) : findbuffer(fname))) == NULL)
		return FALSE;
	curbp = bp;
	if (showbuffer(bp, curwp, WFHARD) != TRUE)
		return FALSE;
	if (bp->b_fname[0] != 0)
		return TRUE;
	return readin(fname);
}

/* ARGSUSED */
int
d_ffotherwindow(int f, int n)
{
	char	fname[NFILEN];
	int	s;
	BUFFER *bp;
	MGWIN  *wp;

	if ((s = d_makename(curwp->w_dotp, fname, sizeof fname)) == ABORT)
		return FALSE;
	if ((bp = (s ? dired_(fname) : findbuffer(fname))) == NULL)
		return FALSE;
	if ((wp = popbuf(bp)) == NULL)
		return FALSE;
	curbp = bp;
	curwp = wp;
	if (bp->b_fname[0] != 0)
		return TRUE;	/* never true for dired buffers */
	return readin(fname);
}

/* ARGSUSED */
int
d_expunge(int f, int n)
{
	LINE	*lp, *nlp;
	char	fname[NFILEN];

	for (lp = lforw(curbp->b_linep); lp != curbp->b_linep; lp = nlp) {
		nlp = lforw(lp);
		if (llength(lp) && lgetc(lp, 0) == 'D') {
			switch (d_makename(lp, fname, sizeof fname)) {
			case ABORT:
				ewprintf("Bad line in dired buffer");
				return FALSE;
			case FALSE:
				if (unlink(fname) < 0) {
					ewprintf("Could not delete '%s'", fname);
					return FALSE;
				}
				break;
			case TRUE:
				if (rmdir(fname) < 0) {
					ewprintf("Could not delete directory '%s'",
					    fname);
					return FALSE;
				}
				break;
			}
			lfree(lp);
			curwp->w_flag |= WFHARD;
		}
	}
	return TRUE;
}

/* ARGSUSED */
int
d_copy(int f, int n)
{
	char	frname[NFILEN], toname[NFILEN];
	int	stat;

	if (d_makename(curwp->w_dotp, frname, sizeof frname) != FALSE) {
		ewprintf("Not a file");
		return FALSE;
	}
	if ((stat = eread("Copy %s to: ", toname, NFILEN, EFNEW | EFCR, frname))
	    != TRUE)
		return stat;
	return copy(frname, toname) >= 0;
}

/* ARGSUSED */
int
d_rename(int f, int n)
{
	char	frname[NFILEN], toname[NFILEN];
	int	stat;

	if (d_makename(curwp->w_dotp, frname, sizeof frname) != FALSE) {
		ewprintf("Not a file");
		return FALSE;
	}
	if ((stat = eread("Rename %s to: ", toname, NFILEN, EFNEW | EFCR,
	    frname)) != TRUE)
		return stat;
	return rename(frname, toname) >= 0;
}
#endif
