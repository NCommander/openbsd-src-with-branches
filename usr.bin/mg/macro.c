/*	$OpenBSD: macro.c,v 1.7 2004/02/01 22:26:41 vincent Exp $	*/

/*
 *	Keyboard macros.
 */

#ifndef NO_MACRO
#include "def.h"
#include "key.h"
#include "macro.h"

int inmacro = FALSE;
int macrodef = FALSE;
int macrocount = 0;

LINE *maclhead = NULL;
LINE *maclcur;

union macrodef macro[MAXMACRO];

/* ARGSUSED */
int
definemacro(int f, int n)
{
	LINE	*lp1, *lp2;

	macrocount = 0;

	if (macrodef) {
		ewprintf("already defining macro");
		return (macrodef = FALSE);
	}

	/* free lines allocated for string arguments */
	if (maclhead != NULL) {
		for (lp1 = maclhead->l_fp; lp1 != maclhead; lp1 = lp2) {
			lp2 = lp1->l_fp;
			free((char *)lp1);
		}
		free((char *)lp1);
	}

	if ((maclhead = lp1 = lalloc(0)) == NULL)
		return (FALSE);

	ewprintf("Defining Keyboard Macro...");
	maclcur = lp1->l_fp = lp1->l_bp = lp1;
	return (macrodef = TRUE);
}

/* ARGSUSED */
int
finishmacro(int f, int n)
{
	macrodef = FALSE;
	ewprintf("End Keyboard Macro Definition");
	return (TRUE);
}

/* ARGSUSED */
int
executemacro(int f, int n)
{
	int	 i, j, flag, num;
	PF	 funct;

	if (macrodef ||
	    (macrocount >= MAXMACRO && macro[MAXMACRO].m_funct != finishmacro))
		return (FALSE);

	if (macrocount == 0)
		return (TRUE);

	inmacro = TRUE;

	for (i = n; i > 0; i--) {
		maclcur = maclhead->l_fp;
		flag = 0;
		num = 1;
		for (j = 0; j < macrocount - 1; j++) {
			funct = macro[j].m_funct;
			if (funct == universal_argument) {
				flag = FFARG;
				num = macro[++j].m_count;
				continue;
			}
			if ((*funct)(flag, num) != TRUE) {
				inmacro = FALSE;
				return (FALSE);
			}
			lastflag = thisflag;
			thisflag = 0;
			flag = 0;
			num = 1;
		}
	}
	inmacro = FALSE;
	return (TRUE);
}
#endif	/* NO_MACRO */
