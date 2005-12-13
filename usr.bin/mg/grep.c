/*	$OpenBSD: grep.c,v 1.24 2005/11/18 20:56:53 deraadt Exp $	*/
/*
 * Copyright (c) 2001 Artur Grabowski <art@openbsd.org>.
 * Copyright (c) 2005 Kjell Wooding <kjell@openbsd.org>.
 * All rights reserved.
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

#include <sys/types.h>
#include <ctype.h>
#include <libgen.h>
#include <time.h>

static int	 compile_goto_error(int, int);
int		 next_error(int, int);
static int	 grep(int, int);
static int	 compile(int, int);
static int	 gid(int, int);
static struct buffer	*compile_mode(const char *, const char *, const char *);
static int	 getbufcwd(char *, size_t);
static int	 xlint(int, int);

void grep_init(void);

static char compile_last_command[NFILEN] = "make ";

/*
 * Hints for next-error
 *
 * XXX - need some kind of callback to find out when those get killed.
 */
struct mgwin	*compile_win;
struct buffer	*compile_buffer;

static PF compile_pf[] = {
	compile_goto_error
};

static struct KEYMAPE (1 + IMAPEXT) compilemap = {
	1,
	1 + IMAPEXT,
	rescan,
	{
		{ CCHR('M'), CCHR('M'), compile_pf, NULL }
	}
};

void
grep_init(void)
{
	funmap_add(compile_goto_error, "compile-goto-error");
	funmap_add(next_error, "next-error");
	funmap_add(grep, "grep");
	funmap_add(xlint, "lint");
	funmap_add(compile, "compile");
	funmap_add(gid, "gid");
	maps_add((KEYMAP *)&compilemap, "compile");
}

/* ARGSUSED */
static int
grep(int f, int n)
{
	char	 command[NFILEN + 21];
	char	 cprompt[NFILEN], *bufp;
	struct buffer	*bp;
	struct mgwin	*wp;
	char	 path[NFILEN];

	/* get buffer cwd */
	if (getbufcwd(path, sizeof(path)) == FALSE) {
		ewprintf("Failed. "
		    "Can't get working directory of current buffer.");
		return (FALSE);
	}

	(void)strlcpy(cprompt, "grep -n ", sizeof(cprompt));
	if ((bufp = eread("Run grep: ", cprompt, NFILEN,
	    EFDEF | EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	(void)snprintf(command, sizeof(command), "%s /dev/null", bufp);

	if ((bp = compile_mode("*grep*", command, path)) == NULL)
		return (FALSE);
	if ((wp = popbuf(bp)) == NULL)
		return (FALSE);
	curbp = bp;
	compile_win = curwp = wp;
	return (TRUE);
}

/* ARGSUSED */
static int
xlint(int f, int n)
{
	char	 command[NFILEN + 16];
	char	 cprompt[NFILEN], *bufp;
	struct buffer	*bp;
	struct mgwin	*wp;
	char	 path[NFILEN];

	/* get buffer cwd */
	if (getbufcwd(path, sizeof(path)) == FALSE) {
		ewprintf("Failed. "
		    "Can't get working directory of current buffer.");
		return (FALSE);
	}

	(void)strlcpy(cprompt, "make lint ", sizeof(cprompt));
	if ((bufp = eread("Run lint: ", cprompt, NFILEN,
	    EFDEF | EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	(void)snprintf(command, sizeof(command), "%s 2>&1", bufp);

	if ((bp = compile_mode("*lint*", command, path)) == NULL)
		return (FALSE);
	if ((wp = popbuf(bp)) == NULL)
		return (FALSE);
	curbp = bp;
	compile_win = curwp = wp;
	return (TRUE);
}

/* ARGSUSED */
static int
compile(int f, int n)
{
	char	 command[NFILEN + 20];
	char	 cprompt[NFILEN], *bufp;
	struct buffer	*bp;
	struct mgwin	*wp;
	char	 path[NFILEN];

	/* get buffer cwd */
	if (getbufcwd(path, sizeof(path)) == FALSE) {
		ewprintf("Failed. "
		    "Can't get working directory of current buffer.");
		return (FALSE);
	}

	(void)strlcpy(cprompt, compile_last_command, sizeof(cprompt));
	if ((bufp = eread("Compile command: ", cprompt, NFILEN,
	    EFDEF | EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	if (savebuffers(f, n) == ABORT)
		return (ABORT);
	(void)strlcpy(compile_last_command, bufp, sizeof(compile_last_command));

	(void)snprintf(command, sizeof(command), "%s 2>&1", bufp);

	if ((bp = compile_mode("*compile*", command, path)) == NULL)
		return (FALSE);
	if ((wp = popbuf(bp)) == NULL)
		return (FALSE);
	curbp = bp;
	compile_win = curwp = wp;
	return (TRUE);
}

/* id-utils foo. */
/* ARGSUSED */
static int
gid(int f, int n)
{
	char	 command[NFILEN + 20];
	char	 cprompt[NFILEN], c, *bufp;
	struct buffer	*bp;
	struct mgwin	*wp;
	int	 i, j;
	char	 path[NFILEN];

	/* get buffer cwd */
	if (getbufcwd(path, sizeof(path)) == FALSE) {
		ewprintf("Failed. "
		    "Can't get working directory of current buffer.");
		return (FALSE);
	}

	/* catch ([^\s(){}]+)[\s(){}]* */

	i = curwp->w_doto;
	/* Skip backwards over delimiters we are currently on */
	while (i > 0) {
		c = lgetc(curwp->w_dotp, i);
		if (isalnum(c) || c == '_')
			break;

		i--;
	}

	/* Skip the symbol itself */
	for (; i > 0; i--) {
		c = lgetc(curwp->w_dotp, i - 1);
		if (!isalnum(c) && c != '_')
			break;
	}
	/* Fill the symbol in cprompt[] */
	for (j = 0; j < sizeof(cprompt) - 1 && i < llength(curwp->w_dotp);
	    j++, i++) {
		c = lgetc(curwp->w_dotp, i);
		if (!isalnum(c) && c != '_')
			break;
		cprompt[j] = c;
	}
	cprompt[j] = '\0';

	if ((bufp = eread("Run gid (with args): ", cprompt, NFILEN,
	    (j ? EFDEF : 0) | EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	(void)snprintf(command, sizeof(command), "gid %s", cprompt);

	if ((bp = compile_mode("*gid*", command, path)) == NULL)
		return (FALSE);
	if ((wp = popbuf(bp)) == NULL)
		return (FALSE);
	curbp = bp;
	compile_win = curwp = wp;
	return (TRUE);
}

struct buffer *
compile_mode(const char *name, const char *command, const char *path)
{
	struct buffer	*bp;
	FILE	*pipe;
	char	*buf;
	size_t	 len;
	int	 ret;
	char	 cwd[NFILEN];
	char	 timestr[NTIME];
	time_t	 t;

	bp = bfind(name, TRUE);
	if (bclear(bp) != TRUE)
		return (NULL);

	addlinef(bp, "cd %s", path);
	addline(bp, command);
	addline(bp, "");

	if (getcwd(cwd, sizeof(cwd)) == NULL)
		panic("Can't get current directory!");
	if (chdir(path) == -1) {
		ewprintf("Can't change dir to %s", path);
		return (NULL);
	}
	if ((pipe = popen(command, "r")) == NULL) {
		ewprintf("Problem opening pipe");
		return (NULL);
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
	t = time(NULL);
	strftime(timestr, sizeof(timestr), "%a %b %e %T %Y", localtime(&t));
	addline(bp, "");
	if (ret != 0)
		addlinef(bp, "Command exited abnormally with code %d"
		    " at %s", ret, timestr);
	else
		addlinef(bp, "Command finished at %s", timestr);

	bp->b_dotp = lforw(bp->b_linep);	/* go to first line */
	bp->b_modes[0] = name_mode("fundamental");
	bp->b_modes[1] = name_mode("compile");
	bp->b_nmodes = 1;

	compile_buffer = bp;

	if (chdir(cwd) == -1) {
		ewprintf("Can't change dir back to %s", cwd);
		return (NULL);
	}
	return (bp);
}

/* ARGSUSED */
static int
compile_goto_error(int f, int n)
{
	struct buffer	*bp;
	struct mgwin	*wp;
	char	*fname, *line, *lp, *ln;
	int	 lineno, len;
	char	*adjf;
	const char *errstr;
	struct line	*last;

	compile_win = curwp;
	compile_buffer = curbp;
	last = lback(compile_buffer->b_linep);

 retry:
	/* last line is compilation result */
	if (curwp->w_dotp == last)
		return (FALSE);

	len = llength(curwp->w_dotp);

	if ((line = malloc(len + 1)) == NULL)
		return (FALSE);

	(void)memcpy(line, curwp->w_dotp->l_text, len);
	line[len] = '\0';

	lp = line;
	if ((fname = strsep(&lp, ":")) == NULL || *fname == '\0')
		goto fail;
	if ((ln = strsep(&lp, ":")) == NULL || *ln == '\0')
		goto fail;
	lineno = (int)strtonum(ln, INT_MIN, INT_MAX, &errstr);
	if (errstr)
		goto fail;

	adjf = adjustname(fname);
	free(line);

	if (adjf == NULL)
		return (FALSE);

	if ((bp = findbuffer(adjf)) == NULL)
		return (FALSE);
	if ((wp = popbuf(bp)) == NULL)
		return (FALSE);
	curbp = bp;
	curwp = wp;
	if (bp->b_fname[0] == 0)
		readin(adjf);
	gotoline(FFARG, lineno);
	return (TRUE);
fail:
	free(line);
	if (curwp->w_dotp != lback(curbp->b_linep)) {
		curwp->w_dotp = lforw(curwp->w_dotp);
		curwp->w_flag |= WFMOVE;
		goto retry;
	}
	ewprintf("No more hits");
	return (FALSE);
}

/* ARGSUSED */
int
next_error(int f, int n)
{
	if (compile_win == NULL || compile_buffer == NULL) {
		ewprintf("No compilation active");
		return (FALSE);
	}
	curwp = compile_win;
	curbp = compile_buffer;
	if (curwp->w_dotp == lback(curbp->b_linep)) {
		ewprintf("No more hits");
		return (FALSE);
	}
	curwp->w_dotp = lforw(curwp->w_dotp);
	curwp->w_flag |= WFMOVE;

	return (compile_goto_error(f, n));
}

/*
 * Return the working directory for the current buffer, terminated
 * with a '/'. First, try to extract it from the current buffer's
 * filename. If that fails, use global cwd.
 */
static int
getbufcwd(char *path, size_t plen)
{
	char *dname, cwd[NFILEN];
	if (plen == 0)
		goto error;

	if (curbp->b_fname && curbp->b_fname[0] != '\0' &&
	    (dname = dirname(curbp->b_fname)) != NULL) {
		if (strlcpy(path, dname, plen) >= plen)
			goto error;
		if (strlcat(path, "/", plen) >= plen)
			goto error;
	} else {
		if ((dname = getcwd(cwd, sizeof(cwd))) == NULL)
			goto error;
		if (strlcpy(path, dname, plen) >= plen)
			goto error;
	}
	return (TRUE);
error:
	path = NULL;
	return (FALSE);
}
