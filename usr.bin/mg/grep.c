/*	$OpenBSD$	*/
/*
 * Copyright (c) 2001 Artur Grabowski <art@openbsd.org>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "def.h"
#include "kbd.h"
#include "funmap.h"

static int	compile_goto_error(int f, int n);
static int	next_error(int f, int n);
static int	grep(int, int);
static int	compile(int, int);
static int	gid(int, int);
static BUFFER	*compile_mode(char *name, char *command);


void grep_init(void);

/*
 * Hints for next-error
 *
 * XXX - need some kind of callback to find out when those get killed.
 */
MGWIN *compile_win;
BUFFER *compile_buffer;

static PF compile_pf[] = {
	compile_goto_error,
};

static struct KEYMAPE (1 + IMAPEXT) compilemap = {
	1,
	1 + IMAPEXT,
	rescan,
	{
		{ CCHR('M'), CCHR('M'), compile_pf, NULL },
	}
};

void
grep_init(void)
{
	funmap_add(compile_goto_error, "compile-goto-error");
	funmap_add(next_error, "next-error");
	funmap_add(grep, "grep");
	funmap_add(compile, "compile");
	funmap_add(gid, "gid");
	maps_add((KEYMAP *)&compilemap, "compile");
}

static int
grep(int f, int n)
{
	char command[NFILEN + 20];
	char prompt[NFILEN];
	BUFFER *bp;
	MGWIN *wp;

	strcpy(prompt, "grep -n ");
	if (eread("Run grep: ", prompt, NFILEN, EFDEF|EFNEW|EFCR) == ABORT)
		return ABORT;

	sprintf(command, "%s /dev/null", prompt);

	if ((bp = compile_mode("*grep*", command)) == NULL)
		return FALSE;
	if ((wp = popbuf(bp)) == NULL)
		return FALSE;
	curbp = bp;
	compile_win = curwp = wp;
	return TRUE;
}

static int
compile(int f, int n)
{
	char command[NFILEN + 20];
	char prompt[NFILEN];
	BUFFER *bp;
	MGWIN *wp;

	strcpy(prompt, "make ");
	if (eread("Compile command: ", prompt, NFILEN, EFDEF|EFNEW|EFCR) == ABORT)
		return ABORT;

	sprintf(command, "%s 2>&1", prompt);

	if ((bp = compile_mode("*compile*", command)) == NULL)
		return FALSE;
	if ((wp = popbuf(bp)) == NULL)
		return FALSE;
	curbp = bp;
	compile_win = curwp = wp;
	return TRUE;
}

/* id-utils foo. */
static int
gid(int f, int n)
{
	char command[NFILEN + 20];
	char prompt[NFILEN];
	BUFFER *bp;
	MGWIN *wp;

	if (eread("Run gid (with args): ", prompt, NFILEN, EFNEW|EFCR) == ABORT)
		return ABORT;

	sprintf(command, "gid %s", prompt);

	if ((bp = compile_mode("*gid*", command)) == NULL)
		return FALSE;
	if ((wp = popbuf(bp)) == NULL)
		return FALSE;
	curbp = bp;
	compile_win = curwp = wp;
	return TRUE;
}

BUFFER *
compile_mode(char *name, char *command)
{
	BUFFER *bp;
	FILE *pipe;
	char *buf;
	size_t len;
	int ret;

	bp = bfind(name, TRUE);
	if (bclear(bp) != TRUE)
		return NULL;

	addlinef(bp, "Running (%s).", command); 
	addline(bp, "");

	if ((pipe = popen(command, "r")) == NULL) {
		ewprintf("Problem opening pipe");
		return NULL;
	}
	/*
	 * We know that our commands are nice and the last line will end with
	 * a \n, so we don't need to try to deal with the last line problem
	 * in fgetln.
	 */
	while ((buf = fgetln(pipe, &len)) != NULL) {
		buf[len - 1] = '\0';
		addline(bp, buf);
	}
	ret = pclose(pipe);
	addline(bp, "");
	addlinef(bp, "Command (%s) completed %s.", command,
		ret == 0 ? "successfully" : "with errors");
	bp->b_dotp = lforw(bp->b_linep);	/* go to first line */
	bp->b_modes[0] = name_mode("fundamental");
	bp->b_modes[1] = name_mode("compile");
	bp->b_nmodes = 1;

	compile_buffer = bp;

	return bp;
}

static int
compile_goto_error(int f, int n)
{
	BUFFER *bp;
	MGWIN *wp;
	char *fname, *line, *lp, *ln, *lp1;
	int lineno, len;
	char *adjf;

	compile_win = curwp;
	compile_buffer = curbp;

retry:
	len = llength(curwp->w_dotp);

	if ((line = malloc(len + 1)) == NULL)
		return FALSE;

	memcpy(line, curwp->w_dotp->l_text, len);
	line[len] = '\0';

	lp = line;
	if ((fname = strsep(&lp, ":")) == NULL)
		goto fail;
	if ((ln = strsep(&lp, ":")) == NULL)
		goto fail;
	lineno = strtol(ln, &lp1, 10);
	if (lp != lp1 + 1)
		goto fail;
	free(line);

	adjf = adjustname(fname);
	if ((bp = findbuffer(adjf)) == NULL)
		return FALSE;
	if ((wp = popbuf(bp)) == NULL)
		return FALSE;
	curbp = bp;
	curwp = wp;
	if (bp->b_fname[0] == 0)
		readin(adjf);
	gotoline(FFARG, lineno);
	return TRUE;
fail:
 	free(line);
	if (curwp->w_dotp != lback(curbp->b_linep)) {
		curwp->w_dotp = lforw(curwp->w_dotp);
		curwp->w_flag |= WFMOVE;
		goto retry;
	}
	ewprintf("No more hits");
 	return FALSE;
}

static int
next_error(int f, int n)
{
	if (compile_win == NULL || compile_buffer == NULL) {
		ewprintf("No compilation active");
		return FALSE;
	}
	curwp = compile_win;
	curbp = compile_buffer;
	if (curwp->w_dotp == lback(curbp->b_linep)) {
		ewprintf("No more hits");
		return FALSE;
	}
	curwp->w_dotp = lforw(curwp->w_dotp);
	curwp->w_flag |= WFMOVE;

	return compile_goto_error(f, n);
}
