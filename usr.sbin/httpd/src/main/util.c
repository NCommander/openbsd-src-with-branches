/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000-2003 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * Portions of this software are based upon public domain software
 * originally written at the National Center for Supercomputing Applications,
 * University of Illinois, Urbana-Champaign.
 */

/*
 * util.c: string utility things
 * 
 * 3/21/93 Rob McCool
 * 1995-96 Many changes by the Apache Group
 * 
 */

/* Debugging aid:
 * #define DEBUG            to trace all cfg_open*()/cfg_closefile() calls
 * #define DEBUG_CFG_LINES  to trace every line read from the config files
 */

#include "httpd.h"
#include "http_conf_globals.h"	/* for user_id & group_id */
#include "http_log.h"

/* A bunch of functions in util.c scan strings looking for certain characters.
 * To make that more efficient we encode a lookup table.  The test_char_table
 * is generated automatically by gen_test_char.c.
 */
#include "test_char.h"

/* we assume the folks using this ensure 0 <= c < 256... which means
 * you need a cast to (unsigned char) first, you can't just plug a
 * char in here and get it to work, because if char is signed then it
 * will first be sign extended.
 */
#define TEST_CHAR(c, f)	(test_char_table[(unsigned)(c)] & (f))

void ap_util_init(void)
{
    /* nothing to do... previously there was run-time initialization of
     * test_char_table here
     */
}


API_VAR_EXPORT const char ap_month_snames[12][4] =
{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
API_VAR_EXPORT const char ap_day_snames[7][4] =
{
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

API_EXPORT(char *) ap_get_time(void)
{
    time_t t;
    char *time_string;

    t = time(NULL);
    time_string = ctime(&t);
    time_string[strlen(time_string) - 1] = '\0';
    return (time_string);
}

/*
 * Examine a field value (such as a media-/content-type) string and return
 * it sans any parameters; e.g., strip off any ';charset=foo' and the like.
 */
API_EXPORT(char *) ap_field_noparam(pool *p, const char *intype)
{
    const char *semi;

    if (intype == NULL) return NULL;

    semi = strchr(intype, ';');
    if (semi == NULL) {
	return ap_pstrdup(p, intype);
    } 
    else {
	while ((semi > intype) && ap_isspace(semi[-1])) {
	    semi--;
	}
	return ap_pstrndup(p, intype, semi - intype);
    }
}

API_EXPORT(char *) ap_ht_time(pool *p, time_t t, const char *fmt, int gmt)
{
    char ts[MAX_STRING_LEN];
    char tf[MAX_STRING_LEN];
    struct tm *tms;

    tms = (gmt ? gmtime(&t) : localtime(&t));
    if(gmt) {
	/* Convert %Z to "GMT" and %z to "+0000";
	 * on hosts that do not have a time zone string in struct tm,
	 * strftime must assume its argument is local time.
	 */
	const char *f;
	char *strp;
	for(strp = tf, f = fmt; strp < tf + sizeof(tf) - 6 && (*strp = *f)
	    ; f++, strp++) {
	    if (*f != '%') continue;
	    switch (f[1]) {
	    case '%':
		*++strp = *++f;
		break;
	    case 'Z':
		*strp++ = 'G';
		*strp++ = 'M';
		*strp = 'T';
		f++;
		break;
	    case 'z': /* common extension */
		*strp++ = '+';
		*strp++ = '0';
		*strp++ = '0';
		*strp++ = '0';
		*strp = '0';
		f++;
		break;
	    }
	}
	*strp = '\0';
	fmt = tf;
    }

    /* check return code? */
    strftime(ts, MAX_STRING_LEN, fmt, tms);
    ts[MAX_STRING_LEN - 1] = '\0';
    return ap_pstrdup(p, ts);
}

API_EXPORT(char *) ap_gm_timestr_822(pool *p, time_t sec)
{
    struct tm *tms;

    tms = gmtime(&sec);

    /* RFC date format; as strftime '%a, %d %b %Y %T GMT' */
    return ap_psprintf(p,
		"%s, %.2d %s %d %.2d:%.2d:%.2d GMT", ap_day_snames[tms->tm_wday],
		tms->tm_mday, ap_month_snames[tms->tm_mon], tms->tm_year + 1900,
		tms->tm_hour, tms->tm_min, tms->tm_sec);
}

/* What a pain in the ass. */
API_EXPORT(struct tm *) ap_get_gmtoff(int *tz)
{
    time_t tt = time(NULL);
    struct tm *t;

    t = localtime(&tt);
    *tz = (int) (t->tm_gmtoff / 60);
    return t;
}

/* Roy owes Rob beer. */
/* Rob owes Roy dinner. */

/* These legacy comments would make a lot more sense if Roy hadn't
 * replaced the old later_than() routine with util_date.c.
 *
 * Well, okay, they still wouldn't make any sense.
 */

/* Match = 0, NoMatch = 1, Abort = -1
 * Based loosely on sections of wildmat.c by Rich Salz
 * Hmmm... shouldn't this really go component by component?
 */
API_EXPORT(int) ap_strcmp_match(const char *str, const char *exp)
{
    int x, y;

    for (x = 0, y = 0; exp[y]; ++y, ++x) {
	if ((!str[x]) && (exp[y] != '*'))
	    return -1;
	if (exp[y] == '*') {
	    while (exp[++y] == '*');
	    if (!exp[y])
		return 0;
	    while (str[x]) {
		int ret;
		if ((ret = ap_strcmp_match(&str[x++], &exp[y])) != 1)
		    return ret;
	    }
	    return -1;
	}
	else if ((exp[y] != '?') && (str[x] != exp[y]))
	    return 1;
    }
    return (str[x] != '\0');
}

API_EXPORT(int) ap_strcasecmp_match(const char *str, const char *exp)
{
    int x, y;

    for (x = 0, y = 0; exp[y]; ++y, ++x) {
	if ((!str[x]) && (exp[y] != '*'))
	    return -1;
	if (exp[y] == '*') {
	    while (exp[++y] == '*');
	    if (!exp[y])
		return 0;
	    while (str[x]) {
		int ret;
		if ((ret = ap_strcasecmp_match(&str[x++], &exp[y])) != 1)
		    return ret;
	    }
	    return -1;
	}
	else if ((exp[y] != '?') && (ap_tolower(str[x]) != ap_tolower(exp[y])))
	    return 1;
    }
    return (str[x] != '\0');
}

API_EXPORT(int) ap_is_matchexp(const char *str)
{
    register int x;

    for (x = 0; str[x]; x++)
	if ((str[x] == '*') || (str[x] == '?'))
	    return 1;
    return 0;
}

/*
 * Similar to standard strstr() but we ignore case in this version.
 * Based on the strstr() implementation further below.
 */
API_EXPORT(char *) ap_strcasestr(const char *s1, const char *s2)
{
    char *p1, *p2;
    if (*s2 == '\0') {
	/* an empty s2 */
        return((char *)s1);
    }
    while(1) {
	for ( ; (*s1 != '\0') && (ap_tolower(*s1) != ap_tolower(*s2)); s1++);
	if (*s1 == '\0') return(NULL);
	/* found first character of s2, see if the rest matches */
        p1 = (char *)s1;
        p2 = (char *)s2;
        while (ap_tolower(*++p1) == ap_tolower(*++p2)) {
            if (*p1 == '\0') {
                /* both strings ended together */
                return((char *)s1);
            }
        }
        if (*p2 == '\0') {
            /* second string ended, a match */
            break;
        }
	/* didn't find a match here, try starting at next character in s1 */
        s1++;
    }
    return((char *)s1);
}

/*
 * Returns an offsetted pointer in bigstring immediately after
 * prefix. Returns bigstring if bigstring doesn't start with
 * prefix or if prefix is longer than bigstring while still matching.
 * NOTE: pointer returned is relative to bigstring, so we
 * can use standard pointer comparisons in the calling function
 * (eg: test if ap_stripprefix(a,b) == a)
 */
API_EXPORT(char *) ap_stripprefix(const char *bigstring, const char *prefix)
{
    char *p1;
    if (*prefix == '\0') {
        return( (char *)bigstring);
    }
    p1 = (char *)bigstring;
    while(*p1 && *prefix) {
        if (*p1++ != *prefix++)
            return( (char *)bigstring);
    }
    if (*prefix == '\0')
        return(p1);
    else /* hit the end of bigstring! */
        return( (char *)bigstring);
}

/* 
 * Apache stub function for the regex libraries regexec() to make sure the
 * whole regex(3) API is available through the Apache (exported) namespace.
 * This is especially important for the DSO situations of modules.
 * DO NOT MAKE A MACRO OUT OF THIS FUNCTION!
 */
API_EXPORT(int) ap_regexec(const regex_t *preg, const char *string,
                           size_t nmatch, regmatch_t pmatch[], int eflags)
{
    return regexec(preg, string, nmatch, pmatch, eflags);
}

API_EXPORT(size_t) ap_regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size)
{
    return regerror(errcode, preg, errbuf, errbuf_size);
}


/* This function substitutes for $0-$9, filling in regular expression
 * submatches. Pass it the same nmatch and pmatch arguments that you
 * passed ap_regexec(). pmatch should not be greater than the maximum number
 * of subexpressions - i.e. one more than the re_nsub member of regex_t.
 *
 * input should be the string with the $-expressions, source should be the
 * string that was matched against.
 *
 * It returns the substituted string, or NULL on error.
 *
 * Parts of this code are based on Henry Spencer's regsub(), from his
 * AT&T V8 regexp package.
 */

API_EXPORT(char *) ap_pregsub(pool *p, const char *input, const char *source,
			   size_t nmatch, regmatch_t pmatch[])
{
    const char *src = input;
    char *dest, *dst;
    char c;
    size_t no;
    int len;

    if (!source)
	return NULL;
    if (!nmatch)
	return ap_pstrdup(p, src);

    /* First pass, find the size */

    len = 0;

    while ((c = *src++) != '\0') {
	if (c == '&')
	    no = 0;
	else if (c == '$' && ap_isdigit(*src))
	    no = *src++ - '0';
	else
	    no = 10;

	if (no > 9) {		/* Ordinary character. */
	    if (c == '\\' && (*src == '$' || *src == '&'))
		c = *src++;
	    len++;
	}
	else if (no < nmatch && pmatch[no].rm_so < pmatch[no].rm_eo) {
	    len += pmatch[no].rm_eo - pmatch[no].rm_so;
	}

    }

    dest = dst = ap_pcalloc(p, len + 1);

    /* Now actually fill in the string */

    src = input;

    while ((c = *src++) != '\0') {
	if (c == '&')
	    no = 0;
	else if (c == '$' && ap_isdigit(*src))
	    no = *src++ - '0';
	else
	    no = 10;

	if (no > 9) {		/* Ordinary character. */
	    if (c == '\\' && (*src == '$' || *src == '&'))
		c = *src++;
	    *dst++ = c;
	}
	else if (no < nmatch && pmatch[no].rm_so < pmatch[no].rm_eo) {
	    len = pmatch[no].rm_eo - pmatch[no].rm_so;
	    memcpy(dst, source + pmatch[no].rm_so, len);
	    dst += len;
	}

    }
    *dst = '\0';

    return dest;
}

/*
 * Parse .. so we don't compromise security
 */
API_EXPORT(void) ap_getparents(char *name)
{
    int l, w;

    /* Four passes, as per RFC 1808 */
    /* a) remove ./ path segments */

    for (l = 0, w = 0; name[l] != '\0';) {
	if (name[l] == '.' && name[l + 1] == '/' && (l == 0 || name[l - 1] == '/'))
	    l += 2;
	else
	    name[w++] = name[l++];
    }

    /* b) remove trailing . path, segment */
    if (w == 1 && name[0] == '.')
	w--;
    else if (w > 1 && name[w - 1] == '.' && name[w - 2] == '/')
	w--;
    name[w] = '\0';

    /* c) remove all xx/../ segments. (including leading ../ and /../) */
    l = 0;

    while (name[l] != '\0') {
	if (name[l] == '.' && name[l + 1] == '.' && name[l + 2] == '/' &&
	    (l == 0 || name[l - 1] == '/')) {
	    register int m = l + 3, n;

	    l = l - 2;
	    if (l >= 0) {
		while (l >= 0 && name[l] != '/')
		    l--;
		l++;
	    }
	    else
		l = 0;
	    n = l;
	    while ((name[n] = name[m]))
		(++n, ++m);
	}
	else
	    ++l;
    }

    /* d) remove trailing xx/.. segment. */
    if (l == 2 && name[0] == '.' && name[1] == '.')
	name[0] = '\0';
    else if (l > 2 && name[l - 1] == '.' && name[l - 2] == '.' && name[l - 3] == '/') {
	l = l - 4;
	if (l >= 0) {
	    while (l >= 0 && name[l] != '/')
		l--;
	    l++;
	}
	else
	    l = 0;
	name[l] = '\0';
    }
}

API_EXPORT(void) ap_no2slash(char *name)
{
    char *d, *s;

    s = d = name;

    while (*s) {
	if ((*d++ = *s) == '/') {
	    do {
		++s;
	    } while (*s == '/');
	}
	else {
	    ++s;
	}
    }
    *d = '\0';
}


/*
 * copy at most n leading directories of s into d
 * d should be at least as large as s plus 1 extra byte
 * assumes n > 0
 * the return value is the ever useful pointer to the trailing \0 of d
 *
 * examples:
 *    /a/b, 1  ==> /
 *    /a/b, 2  ==> /a/
 *    /a/b, 3  ==> /a/b/
 *    /a/b, 4  ==> /a/b/
 *
 * MODIFIED FOR HAVE_DRIVE_LETTERS and NETWARE environments, 
 * so that if n == 0, "/" is returned in d with n == 1 
 * and s == "e:/test.html", "e:/" is returned in d
 * *** See also directory_walk in src/main/http_request.c
 */
API_EXPORT(char *) ap_make_dirstr_prefix(char *d, const char *s, int n)
{
    for (;;) {
	*d = *s;
	if (*d == '\0') {
	    *d = '/';
	    break;
	}
	if (*d == '/' && (--n) == 0)
	    break;
	++d;
	++s;
    }
    *++d = 0;
    return (d);
}


/*
 * return the parent directory name including trailing / of the file s
 */
API_EXPORT(char *) ap_make_dirstr_parent(pool *p, const char *s)
{
    char *last_slash = strrchr(s, '/');
    char *d;
    int l;

    if (last_slash == NULL) {
	/* XXX: well this is really broken if this happens */
	return (ap_pstrdup(p, "/"));
    }
    l = (last_slash - s) + 1;
    d = ap_palloc(p, l + 1);
    memcpy(d, s, l);
    d[l] = 0;
    return (d);
}


/*
 * This function is deprecated.  Use one of the preceding two functions
 * which are faster.
 */
API_EXPORT(char *) ap_make_dirstr(pool *p, const char *s, int n)
{
    register int x, f;
    char *res;

    for (x = 0, f = 0; s[x]; x++) {
	if (s[x] == '/')
	    if ((++f) == n) {
		res = ap_palloc(p, x + 2);
		memcpy(res, s, x);
		res[x] = '/';
		res[x + 1] = '\0';
		return res;
	    }
    }

    if (s[strlen(s) - 1] == '/')
	return ap_pstrdup(p, s);
    else
	return ap_pstrcat(p, s, "/", NULL);
}

API_EXPORT(int) ap_count_dirs(const char *path)
{
    register int x, n;

    for (x = 0, n = 0; path[x]; x++)
	if (path[x] == '/')
	    n++;
    return n;
}


API_EXPORT(void) ap_chdir_file(const char *file)
{
    const char *x;
    char buf[HUGE_STRING_LEN];

    x = strrchr(file, '/');
    if (x == NULL) {
	chdir(file);
    }
    else if (x - file < sizeof(buf) - 1) {
	memcpy(buf, file, x - file);
	buf[x - file] = '\0';
	chdir(buf);
    }
    /* XXX: well, this is a silly function, no method of reporting an
     * error... ah well. */
}

API_EXPORT(char *) ap_getword_nc(pool *atrans, char **line, char stop)
{
    return ap_getword(atrans, (const char **) line, stop);
}

API_EXPORT(char *) ap_getword(pool *atrans, const char **line, char stop)
{
    char *pos = strchr(*line, stop);
    char *res;

    if (!pos) {
	res = ap_pstrdup(atrans, *line);
	*line += strlen(*line);
	return res;
    }

    res = ap_pstrndup(atrans, *line, pos - *line);

    while (*pos == stop) {
	++pos;
    }

    *line = pos;

    return res;
}

API_EXPORT(char *) ap_getword_white_nc(pool *atrans, char **line)
{
    return ap_getword_white(atrans, (const char **) line);
}

API_EXPORT(char *) ap_getword_white(pool *atrans, const char **line)
{
    int pos = -1, x;
    char *res;

    for (x = 0; (*line)[x]; x++) {
	if (ap_isspace((*line)[x])) {
	    pos = x;
	    break;
	}
    }

    if (pos == -1) {
	res = ap_pstrdup(atrans, *line);
	*line += strlen(*line);
	return res;
    }

    res = ap_palloc(atrans, pos + 1);
    ap_cpystrn(res, *line, pos + 1);

    while (ap_isspace((*line)[pos]))
	++pos;

    *line += pos;

    return res;
}

API_EXPORT(char *) ap_getword_nulls_nc(pool *atrans, char **line, char stop)
{
    return ap_getword_nulls(atrans, (const char **) line, stop);
}

API_EXPORT(char *) ap_getword_nulls(pool *atrans, const char **line, char stop)
{
    char *pos = strchr(*line, stop);
    char *res;

    if (!pos) {
	res = ap_pstrdup(atrans, *line);
	*line += strlen(*line);
	return res;
    }

    res = ap_pstrndup(atrans, *line, pos - *line);

    ++pos;

    *line = pos;

    return res;
}

/* Get a word, (new) config-file style --- quoted strings and backslashes
 * all honored
 */

static char *substring_conf(pool *p, const char *start, int len, char quote)
{
    char *result = ap_palloc(p, len + 2);
    char *resp = result;
    int i;

    for (i = 0; i < len; ++i) {
	if (start[i] == '\\' && (start[i + 1] == '\\'
				 || (quote && start[i + 1] == quote)))
	    *resp++ = start[++i];
	else
	    *resp++ = start[i];
    }

    *resp++ = '\0';
    return result;
}

API_EXPORT(char *) ap_getword_conf_nc(pool *p, char **line)
{
    return ap_getword_conf(p, (const char **) line);
}

API_EXPORT(char *) ap_getword_conf(pool *p, const char **line)
{
    const char *str = *line, *strend;
    char *res;
    char quote;

    while (*str && ap_isspace(*str))
	++str;

    if (!*str) {
	*line = str;
	return "";
    }

    if ((quote = *str) == '"' || quote == '\'') {
	strend = str + 1;
	while (*strend && *strend != quote) {
	    if (*strend == '\\' && strend[1] && strend[1] == quote)
		strend += 2;
	    else
		++strend;
	}
	res = substring_conf(p, str + 1, strend - str - 1, quote);

	if (*strend == quote)
	    ++strend;
    }
    else {
	if (*str == '#')
	    ap_log_error(APLOG_MARK, APLOG_WARNING|APLOG_NOERRNO, NULL, 
			 "Apache does not support line-end comments. Consider using quotes around argument: \"%s\"", str);
	strend = str;
	while (*strend && !ap_isspace(*strend))
	    ++strend;

	res = substring_conf(p, str, strend - str, 0);
    }

    while (*strend && ap_isspace(*strend))
	++strend;
    *line = strend;
    return res;
}

API_EXPORT(int) ap_cfg_closefile(configfile_t *cfp)
{
#ifdef DEBUG
    ap_log_error(APLOG_MARK, APLOG_DEBUG|APLOG_NOERRNO, NULL, 
        "Done with config file %s", cfp->name);
#endif
    return (cfp->close == NULL) ? 0 : cfp->close(cfp->param);
}

/* Common structure that holds the file and pool for ap_pcfg_openfile */
typedef struct {
    struct pool *pool;
    FILE *file;
} poolfile_t;

static int cfg_close(void *param)
{
    poolfile_t *cfp = (poolfile_t *) param;
    return (ap_pfclose(cfp->pool, cfp->file));
}

static int cfg_getch(void *param)
{
    poolfile_t *cfp = (poolfile_t *) param;
    return (fgetc(cfp->file));
}

static void *cfg_getstr(void *buf, size_t bufsiz, void *param)
{
    poolfile_t *cfp = (poolfile_t *) param;
    return (fgets(buf, bufsiz, cfp->file));
}

/* Open a configfile_t as FILE, return open configfile_t struct pointer */
API_EXPORT(configfile_t *) ap_pcfg_openfile(pool *p, const char *name)
{
    configfile_t *new_cfg;
    poolfile_t *new_pfile;
    FILE *file;
    struct stat stbuf;
    int saved_errno;

    if (name == NULL) {
        ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, NULL,
               "Internal error: pcfg_openfile() called with NULL filename");
        return NULL;
    }

    if (!ap_os_is_filename_valid(name)) {
        ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, NULL,
                    "Access to config file %s denied: not a valid filename",
                    name);
	errno = EACCES;
        return NULL;
    }

    file = ap_pfopen(p, name, "r");
#ifdef DEBUG
    saved_errno = errno;
    ap_log_error(APLOG_MARK, APLOG_DEBUG | APLOG_NOERRNO, NULL,
                "Opening config file %s (%s)",
                name, (file == NULL) ? strerror(errno) : "successful");
    errno = saved_errno;
#endif
    if (file == NULL)
        return NULL;

    if (fstat(fileno(file), &stbuf) == 0 &&
        !S_ISREG(stbuf.st_mode) &&
        strcmp(name, "/dev/null") != 0) {
	saved_errno = errno;
        ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, NULL,
                    "Access to file %s denied by server: not a regular file",
                    name);
        ap_pfclose(p, file);
	errno = saved_errno;
        return NULL;
    }

    new_cfg = ap_palloc(p, sizeof(*new_cfg));
    new_pfile = ap_palloc(p, sizeof(*new_pfile));
    new_pfile->file = file;
    new_pfile->pool = p;
    new_cfg->param = new_pfile;
    new_cfg->name = ap_pstrdup(p, name);
    new_cfg->getch = (int (*)(void *)) cfg_getch;
    new_cfg->getstr = (void *(*)(void *, size_t, void *)) cfg_getstr;
    new_cfg->close = (int (*)(void *)) cfg_close;
    new_cfg->line_number = 0;
    return new_cfg;
}


/* Allocate a configfile_t handle with user defined functions and params */
API_EXPORT(configfile_t *) ap_pcfg_open_custom(pool *p, const char *descr,
    void *param,
    int(*getch)(void *param),
    void *(*getstr) (void *buf, size_t bufsiz, void *param),
    int(*close_func)(void *param))
{
    configfile_t *new_cfg = ap_palloc(p, sizeof(*new_cfg));
#ifdef DEBUG
    ap_log_error(APLOG_MARK, APLOG_DEBUG | APLOG_NOERRNO, NULL, "Opening config handler %s", descr);
#endif
    new_cfg->param = param;
    new_cfg->name = descr;
    new_cfg->getch = getch;
    new_cfg->getstr = getstr;
    new_cfg->close = close_func;
    new_cfg->line_number = 0;
    return new_cfg;
}


/* Read one character from a configfile_t */
API_EXPORT(int) ap_cfg_getc(configfile_t *cfp)
{
    register int ch = cfp->getch(cfp->param);
    if (ch == LF) 
	++cfp->line_number;
    return ch;
}


/* Read one line from open configfile_t, strip LF, increase line number */
/* If custom handler does not define a getstr() function, read char by char */
API_EXPORT(int) ap_cfg_getline(char *buf, size_t bufsize, configfile_t *cfp)
{
    /* If a "get string" function is defined, use it */
    if (cfp->getstr != NULL) {
	char *src, *dst;
	char *cp;
	char *cbuf = buf;
	size_t cbufsize = bufsize;

	while (1) {
	    ++cfp->line_number;
	    if (cfp->getstr(cbuf, cbufsize, cfp->param) == NULL)
		return 1;

	    /*
	     *  check for line continuation,
	     *  i.e. match [^\\]\\[\r]\n only
	     */
	    cp = cbuf;
	    while (cp < cbuf+cbufsize && *cp != '\0')
		cp++;
	    if (cp > cbuf && cp[-1] == LF) {
		cp--;
		if (cp > cbuf && cp[-1] == CR)
		    cp--;
		if (cp > cbuf && cp[-1] == '\\') {
		    cp--;
		    if (!(cp > cbuf && cp[-1] == '\\')) {
			/*
			 * line continuation requested -
			 * then remove backslash and continue
			 */
			cbufsize -= (cp-cbuf);
			cbuf = cp;
			continue;
		    }
		    else {
			/* 
			 * no real continuation because escaped -
			 * then just remove escape character
			 */
			for ( ; cp < cbuf+cbufsize && *cp != '\0'; cp++)
			    cp[0] = cp[1];
		    }   
		}
	    }
	    break;
	}

	/*
	 * Leading and trailing white space is eliminated completely
	 */
	src = buf;
	while (ap_isspace(*src))
	    ++src;
	/* blast trailing whitespace */
	dst = &src[strlen(src)];
	while (--dst >= src && ap_isspace(*dst))
	    *dst = '\0';
        /* Zap leading whitespace by shifting */
        if (src != buf)
	    for (dst = buf; (*dst++ = *src++) != '\0'; )
	        ;

#ifdef DEBUG_CFG_LINES
	ap_log_error(APLOG_MARK, APLOG_DEBUG|APLOG_NOERRNO, NULL, "Read config: %s", buf);
#endif
	return 0;
    } else {
	/* No "get string" function defined; read character by character */
	register int c;
	register size_t i = 0;

	buf[0] = '\0';
	/* skip leading whitespace */
	do {
	    c = cfp->getch(cfp->param);
	} while (c == '\t' || c == ' ');

	if (c == EOF)
	    return 1;
	
	if(bufsize < 2) {
	    /* too small, assume caller is crazy */
	    return 1;
	}

	while (1) {
	    if ((c == '\t') || (c == ' ')) {
		buf[i++] = ' ';
		while ((c == '\t') || (c == ' '))
		    c = cfp->getch(cfp->param);
	    }
	    if (c == CR) {
		/* silently ignore CR (_assume_ that a LF follows) */
		c = cfp->getch(cfp->param);
	    }
	    if (c == LF) {
		/* increase line number and return on LF */
		++cfp->line_number;
	    }
	    if (c == EOF || c == 0x4 || c == LF || i >= (bufsize - 2)) {
		/* 
		 *  check for line continuation
		 */
		if (i > 0 && buf[i-1] == '\\') {
		    i--;
		    if (!(i > 0 && buf[i-1] == '\\')) {
			/* line is continued */
			c = cfp->getch(cfp->param);
			continue;
		    }
		    /* else nothing needs be done because
		     * then the backslash is escaped and
		     * we just strip to a single one
		     */
		}
		/* blast trailing whitespace */
		while (i > 0 && ap_isspace(buf[i - 1]))
		    --i;
		buf[i] = '\0';
#ifdef DEBUG_CFG_LINES
		ap_log_error(APLOG_MARK, APLOG_DEBUG|APLOG_NOERRNO, NULL, "Read config: %s", buf);
#endif
		return 0;
	    }
	    buf[i] = c;
	    ++i;
	    c = cfp->getch(cfp->param);
	}
    }
}

/* Size an HTTP header field list item, as separated by a comma.
 * The return value is a pointer to the beginning of the non-empty list item
 * within the original string (or NULL if there is none) and the address
 * of field is shifted to the next non-comma, non-whitespace character.
 * len is the length of the item excluding any beginning whitespace.
 */
API_EXPORT(const char *) ap_size_list_item(const char **field, int *len)
{
    const unsigned char *ptr = (const unsigned char *)*field;
    const unsigned char *token;
    int in_qpair, in_qstr, in_com;

    /* Find first non-comma, non-whitespace byte */

    while (*ptr == ',' || ap_isspace(*ptr))
        ++ptr;

    token = ptr;

    /* Find the end of this item, skipping over dead bits */

    for (in_qpair = in_qstr = in_com = 0;
         *ptr && (in_qpair || in_qstr || in_com || *ptr != ',');
         ++ptr) {

        if (in_qpair) {
            in_qpair = 0;
        }
        else {
            switch (*ptr) {
                case '\\': in_qpair = 1;      /* quoted-pair         */
                           break;
                case '"' : if (!in_com)       /* quoted string delim */
                               in_qstr = !in_qstr;
                           break;
                case '(' : if (!in_qstr)      /* comment (may nest)  */
                               ++in_com;
                           break;
                case ')' : if (in_com)        /* end comment         */
                               --in_com;
                           break;
                default  : break;
            }
        }
    }

    if ((*len = (ptr - token)) == 0) {
        *field = (const char *)ptr;
        return NULL;
    }

    /* Advance field pointer to the next non-comma, non-white byte */

    while (*ptr == ',' || ap_isspace(*ptr))
	++ptr;

    *field = (const char *)ptr;
    return (const char *)token;
}

/* Retrieve an HTTP header field list item, as separated by a comma,
 * while stripping insignificant whitespace and lowercasing anything not in
 * a quoted string or comment.  The return value is a new string containing
 * the converted list item (or NULL if none) and the address pointed to by
 * field is shifted to the next non-comma, non-whitespace.
 */
API_EXPORT(char *) ap_get_list_item(pool *p, const char **field)
{
    const char *tok_start;
    const unsigned char *ptr;
    unsigned char *pos;
    char *token;
    int addspace = 0, in_qpair = 0, in_qstr = 0, in_com = 0, tok_len = 0;

    /* Find the beginning and maximum length of the list item so that
     * we can allocate a buffer for the new string and reset the field.
     */
    if ((tok_start = ap_size_list_item(field, &tok_len)) == NULL) {
        return NULL;
    }
    token = ap_palloc(p, tok_len + 1);

    /* Scan the token again, but this time copy only the good bytes.
     * We skip extra whitespace and any whitespace around a '=', '/',
     * or ';' and lowercase normal characters not within a comment,
     * quoted-string or quoted-pair.
     */
    for (ptr = (const unsigned char *)tok_start, pos = (unsigned char *)token;
         *ptr && (in_qpair || in_qstr || in_com || *ptr != ',');
         ++ptr) {

        if (in_qpair) {
            in_qpair = 0;
            *pos++ = *ptr;
        }
        else {
            switch (*ptr) {
                case '\\': in_qpair = 1;
                           if (addspace == 1)
                               *pos++ = ' ';
                           *pos++ = *ptr;
                           addspace = 0;
                           break;
                case '"' : if (!in_com)
                               in_qstr = !in_qstr;
                           if (addspace == 1)
                               *pos++ = ' ';
                           *pos++ = *ptr;
                           addspace = 0;
                           break;
                case '(' : if (!in_qstr)
                               ++in_com;
                           if (addspace == 1)
                               *pos++ = ' ';
                           *pos++ = *ptr;
                           addspace = 0;
                           break;
                case ')' : if (in_com)
                               --in_com;
                           *pos++ = *ptr;
                           addspace = 0;
                           break;
                case ' ' :
                case '\t': if (addspace)
                               break;
                           if (in_com || in_qstr)
                               *pos++ = *ptr;
                           else
                               addspace = 1;
                           break;
                case '=' :
                case '/' :
                case ';' : if (!(in_com || in_qstr))
                               addspace = -1;
                           *pos++ = *ptr;
                           break;
                default  : if (addspace == 1)
                               *pos++ = ' ';
                           *pos++ = (in_com || in_qstr) ? *ptr
                                                        : ap_tolower(*ptr);
                           addspace = 0;
                           break;
            }
        }
    }
    *pos = '\0';

    return token;
}

/* Find an item in canonical form (lowercase, no extra spaces) within
 * an HTTP field value list.  Returns 1 if found, 0 if not found.
 * This would be much more efficient if we stored header fields as
 * an array of list items as they are received instead of a plain string.
 */
API_EXPORT(int) ap_find_list_item(pool *p, const char *line, const char *tok)
{
    const unsigned char *pos;
    const unsigned char *ptr = (const unsigned char *)line;
    int good = 0, addspace = 0, in_qpair = 0, in_qstr = 0, in_com = 0;

    if (!line || !tok)
        return 0;

    do {  /* loop for each item in line's list */

        /* Find first non-comma, non-whitespace byte */

        while (*ptr == ',' || ap_isspace(*ptr))
            ++ptr;

        if (*ptr)
            good = 1;  /* until proven otherwise for this item */
        else
            break;     /* no items left and nothing good found */

        /* We skip extra whitespace and any whitespace around a '=', '/',
         * or ';' and lowercase normal characters not within a comment,
         * quoted-string or quoted-pair.
         */
        for (pos = (const unsigned char *)tok;
             *ptr && (in_qpair || in_qstr || in_com || *ptr != ',');
             ++ptr) {

            if (in_qpair) {
                in_qpair = 0;
                if (good)
                    good = (*pos++ == *ptr);
            }
            else {
                switch (*ptr) {
                    case '\\': in_qpair = 1;
                               if (addspace == 1)
                                   good = good && (*pos++ == ' ');
                               good = good && (*pos++ == *ptr);
                               addspace = 0;
                               break;
                    case '"' : if (!in_com)
                                   in_qstr = !in_qstr;
                               if (addspace == 1)
                                   good = good && (*pos++ == ' ');
                               good = good && (*pos++ == *ptr);
                               addspace = 0;
                               break;
                    case '(' : if (!in_qstr)
                                   ++in_com;
                               if (addspace == 1)
                                   good = good && (*pos++ == ' ');
                               good = good && (*pos++ == *ptr);
                               addspace = 0;
                               break;
                    case ')' : if (in_com)
                                   --in_com;
                               good = good && (*pos++ == *ptr);
                               addspace = 0;
                               break;
                    case ' ' :
                    case '\t': if (addspace || !good)
                                   break;
                               if (in_com || in_qstr)
                                   good = (*pos++ == *ptr);
                               else
                                   addspace = 1;
                               break;
                    case '=' :
                    case '/' :
                    case ';' : if (!(in_com || in_qstr))
                                   addspace = -1;
                               good = good && (*pos++ == *ptr);
                               break;
                    default  : if (!good)
                                   break;
                               if (addspace == 1)
                                   good = (*pos++ == ' ');
                               if (in_com || in_qstr)
                                   good = good && (*pos++ == *ptr);
                               else
                                   good = good && (*pos++ == ap_tolower(*ptr));
                               addspace = 0;
                               break;
                }
            }
        }
        if (good && *pos)
            good = 0;          /* not good if only a prefix was matched */

    } while (*ptr && !good);

    return good;
}


/* Retrieve a token, spacing over it and returning a pointer to
 * the first non-white byte afterwards.  Note that these tokens
 * are delimited by semis and commas; and can also be delimited
 * by whitespace at the caller's option.
 */

API_EXPORT(char *) ap_get_token(pool *p, const char **accept_line, int accept_white)
{
    const char *ptr = *accept_line;
    const char *tok_start;
    char *token;
    int tok_len;

    /* Find first non-white byte */

    while (*ptr && ap_isspace(*ptr))
	++ptr;

    tok_start = ptr;

    /* find token end, skipping over quoted strings.
     * (comments are already gone).
     */

    while (*ptr && (accept_white || !ap_isspace(*ptr))
	   && *ptr != ';' && *ptr != ',') {
	if (*ptr++ == '"')
	    while (*ptr)
		if (*ptr++ == '"')
		    break;
    }

    tok_len = ptr - tok_start;
    token = ap_pstrndup(p, tok_start, tok_len);

    /* Advance accept_line pointer to the next non-white byte */

    while (*ptr && ap_isspace(*ptr))
	++ptr;

    *accept_line = ptr;
    return token;
}


/* find http tokens, see the definition of token from RFC2068 */
API_EXPORT(int) ap_find_token(pool *p, const char *line, const char *tok)
{
    const unsigned char *start_token;
    const unsigned char *s;

    if (!line)
	return 0;

    s = (const unsigned char *)line;
    for (;;) {
	/* find start of token, skip all stop characters, note NUL
	 * isn't a token stop, so we don't need to test for it
	 */
	while (TEST_CHAR(*s, T_HTTP_TOKEN_STOP)) {
	    ++s;
	}
	if (!*s) {
	    return 0;
	}
	start_token = s;
	/* find end of the token */
	while (*s && !TEST_CHAR(*s, T_HTTP_TOKEN_STOP)) {
	    ++s;
	}
	if (!strncasecmp((const char *)start_token, (const char *)tok, s - start_token)) {
	    return 1;
	}
	if (!*s) {
	    return 0;
	}
    }
}


API_EXPORT(int) ap_find_last_token(pool *p, const char *line, const char *tok)
{
    int llen, tlen, lidx;

    if (!line)
	return 0;

    llen = strlen(line);
    tlen = strlen(tok);
    lidx = llen - tlen;

    if ((lidx < 0) ||
	((lidx > 0) && !(ap_isspace(line[lidx - 1]) || line[lidx - 1] == ',')))
	return 0;

    return (strncasecmp(&line[lidx], tok, tlen) == 0);
}

/* c2x takes an unsigned, and expects the caller has guaranteed that
 * 0 <= what < 256... which usually means that you have to cast to
 * unsigned char first, because (unsigned)(char)(x) first goes through
 * signed extension to an int before the unsigned cast.
 *
 * The reason for this assumption is to assist gcc code generation --
 * the unsigned char -> unsigned extension is already done earlier in
 * both uses of this code, so there's no need to waste time doing it
 * again.
 */
static const char c2x_table[] = "0123456789abcdef";

static ap_inline unsigned char *c2x(unsigned what, unsigned char *where)
{
    *where++ = '%';
    *where++ = c2x_table[what >> 4];
    *where++ = c2x_table[what & 0xf];
    return where;
}

/* escape a string for logging */
API_EXPORT(char *) ap_escape_logitem(pool *p, const char *str)
{
    char *ret;
    unsigned char *d;
    const unsigned char *s;

    if (str == NULL)
        return NULL;

    ret = ap_palloc(p, 4 * strlen(str) + 1);	/* Be safe */
    d = (unsigned char *)ret;
    s = (const unsigned char *)str;
    for (; *s; ++s) {

	if (TEST_CHAR(*s, T_ESCAPE_LOGITEM)) {
	    *d++ = '\\';
            switch(*s) {
            case '\b':
                *d++ = 'b';
	        break;
            case '\n':
                *d++ = 'n';
	        break;
            case '\r':
                *d++ = 'r';
	        break;
            case '\t':
                *d++ = 't';
	        break;
            case '\v':
                *d++ = 'v';
	        break;
            case '\\':
            case '"':
                *d++ = *s;
	        break;
	    default:
                c2x(*s, d);
	        *d = 'x';
		d += 3;
	    }
	}
	else
            *d++ = *s;
    }
    *d = '\0';

    return ret;
}

API_EXPORT(size_t) ap_escape_errorlog_item(char *dest, const char *source,
                                           size_t buflen)
{
    unsigned char *d, *ep;
    const unsigned char *s;

    if (!source || !buflen) { /* be safe */
        return 0;
    }

    d = (unsigned char *)dest;
    s = (const unsigned char *)source;
    ep = d + buflen - 1;

    for (; d < ep && *s; ++s) {

        if (TEST_CHAR(*s, T_ESCAPE_LOGITEM)) {
            *d++ = '\\';
            if (d >= ep) {
                --d;
                break;
            }

            switch(*s) {
            case '\b':
                *d++ = 'b';
                break;
            case '\n':
                *d++ = 'n';
                break;
            case '\r':
                *d++ = 'r';
                break;
            case '\t':
                *d++ = 't';
                break;
            case '\v':
                *d++ = 'v';
                break;
            case '\\':
                *d++ = *s;
                break;
            case '"': /* no need for this in error log */
                d[-1] = *s;
                break;
            default:
                if (d >= ep - 2) {
                    ep = --d; /* break the for loop as well */
                    break;
                }
                c2x(*s, d);
                *d = 'x';
                d += 3;
            }
        }
        else {
            *d++ = *s;
        }
    }
    *d = '\0';

    return (d - (unsigned char *)dest);
}

API_EXPORT(char *) ap_escape_shell_cmd(pool *p, const char *str)
{
    char *cmd;
    unsigned char *d;
    const unsigned char *s;

    cmd = ap_palloc(p, 2 * strlen(str) + 1);	/* Be safe */
    d = (unsigned char *)cmd;
    s = (const unsigned char *)str;
    for (; *s; ++s) {


	if (TEST_CHAR(*s, T_ESCAPE_SHELL_CMD)) {
	    *d++ = '\\';
	}
	*d++ = *s;
    }
    *d = '\0';

    return cmd;
}

static char x2c(const char *what)
{
    register char digit;

    digit = ((what[0] >= 'A') ? ((what[0] & 0xdf) - 'A') + 10 : (what[0] - '0'));
    digit *= 16;
    digit += (what[1] >= 'A' ? ((what[1] & 0xdf) - 'A') + 10 : (what[1] - '0'));
    return (digit);
}

/*
 * Unescapes a URL.
 * Returns 0 on success, non-zero on error
 * Failure is due to
 *   bad % escape       returns BAD_REQUEST
 *
 *   decoding %00 -> \0  (the null character)
 *   decoding %2f -> /   (a special character)
 *                      returns NOT_FOUND
 */
API_EXPORT(int) ap_unescape_url(char *url)
{
    register int x, y, badesc, badpath;

    badesc = 0;
    badpath = 0;
    for (x = 0, y = 0; url[y]; ++x, ++y) {
	if (url[y] != '%')
	    url[x] = url[y];
	else {
	    if (!ap_isxdigit(url[y + 1]) || !ap_isxdigit(url[y + 2])) {
		badesc = 1;
		url[x] = '%';
	    }
	    else {
		url[x] = x2c(&url[y + 1]);
		y += 2;
		if (url[x] == '/' || url[x] == '\0')
		    badpath = 1;
	    }
	}
    }
    url[x] = '\0';
    if (badesc)
	return BAD_REQUEST;
    else if (badpath)
	return NOT_FOUND;
    else
	return OK;
}

API_EXPORT(char *) ap_construct_server(pool *p, const char *hostname,
				    unsigned port, const request_rec *r)
{
    if (ap_is_default_port(port, r))
	return ap_pstrdup(p, hostname);
    else {
	return ap_psprintf(p, "%s:%u", hostname, port);
    }
}

/*
 * escape_path_segment() escapes a path segment, as defined in RFC 1808. This
 * routine is (should be) OS independent.
 *
 * os_escape_path() converts an OS path to a URL, in an OS dependent way. In all
 * cases if a ':' occurs before the first '/' in the URL, the URL should be
 * prefixed with "./" (or the ':' escaped). In the case of Unix, this means
 * leaving '/' alone, but otherwise doing what escape_path_segment() does. For
 * efficiency reasons, we don't use escape_path_segment(), which is provided for
 * reference. Again, RFC 1808 is where this stuff is defined.
 *
 * If partial is set, os_escape_path() assumes that the path will be appended to
 * something with a '/' in it (and thus does not prefix "./").
 */

API_EXPORT(char *) ap_escape_path_segment(pool *p, const char *segment)
{
    char *copy = ap_palloc(p, 3 * strlen(segment) + 1);
    const unsigned char *s = (const unsigned char *)segment;
    unsigned char *d = (unsigned char *)copy;
    unsigned c;

    while ((c = *s)) {
	if (TEST_CHAR(c, T_ESCAPE_PATH_SEGMENT)) {
	    d = c2x(c, d);
	}
	else {
	    *d++ = c;
	}
	++s;
    }
    *d = '\0';
    return copy;
}

API_EXPORT(char *) ap_os_escape_path(pool *p, const char *path, int partial)
{
    char *copy = ap_palloc(p, 3 * strlen(path) + 3);
    const unsigned char *s = (const unsigned char *)path;
    unsigned char *d = (unsigned char *)copy;
    unsigned c;

    if (!partial) {
	char *colon = strchr(path, ':');
	char *slash = strchr(path, '/');

	if (colon && (!slash || colon < slash)) {
	    *d++ = '.';
	    *d++ = '/';
	}
    }
    while ((c = *s)) {
	if (TEST_CHAR(c, T_OS_ESCAPE_PATH)) {
	    d = c2x(c, d);
	}
	else {
	    *d++ = c;
	}
	++s;
    }
    *d = '\0';
    return copy;
}

/* ap_escape_uri is now a macro for os_escape_path */

API_EXPORT(char *) ap_escape_html(pool *p, const char *s)
{
    int i, j;
    char *x;

    /* first, count the number of extra characters */
    for (i = 0, j = 0; s[i] != '\0'; i++)
	if (s[i] == '<' || s[i] == '>')
	    j += 3;
	else if (s[i] == '&')
	    j += 4;
	else if (s[i] == '"')
	    j += 5; 

    if (j == 0)
	return ap_pstrndup(p, s, i);

    x = ap_palloc(p, i + j + 1);
    for (i = 0, j = 0; s[i] != '\0'; i++, j++)
	if (s[i] == '<') {
	    memcpy(&x[j], "&lt;", 4);
	    j += 3;
	}
	else if (s[i] == '>') {
	    memcpy(&x[j], "&gt;", 4);
	    j += 3;
	}
	else if (s[i] == '&') {
	    memcpy(&x[j], "&amp;", 5);
	    j += 4;
	}
	else if (s[i] == '"') {
	    memcpy(&x[j], "&quot;", 6);
	    j += 5;
	}
	else
	    x[j] = s[i];

    x[j] = '\0';
    return x;
}

API_EXPORT(int) ap_is_directory(const char *path)
{
    struct stat finfo;

    if (stat(path, &finfo) == -1)
	return 0;		/* in error condition, just return no */

    return (S_ISDIR(finfo.st_mode));
}

/*
 * see ap_is_directory() except this one is symlink aware, so it
 * checks for a "real" directory
 */
API_EXPORT(int) ap_is_rdirectory(const char *path)
{
    struct stat finfo;

    if (lstat(path, &finfo) == -1)
	return 0;		/* in error condition, just return no */

    return ((!(S_ISLNK(finfo.st_mode))) && (S_ISDIR(finfo.st_mode)));
}

API_EXPORT(char *) ap_make_full_path(pool *a, const char *src1,
				  const char *src2)
{
    register int x;

    x = strlen(src1);
    if (x == 0)
	return ap_pstrcat(a, "/", src2, NULL);

    if (src1[x - 1] != '/')
	return ap_pstrcat(a, src1, "/", src2, NULL);
    else
	return ap_pstrcat(a, src1, src2, NULL);
}

/*
 * Check for an absoluteURI syntax (see section 3.2 in RFC2068).
 */
API_EXPORT(int) ap_is_url(const char *u)
{
    register int x;

    for (x = 0; u[x] != ':'; x++) {
	if ((!u[x]) ||
	    ((!ap_isalpha(u[x])) && (!ap_isdigit(u[x])) &&
	     (u[x] != '+') && (u[x] != '-') && (u[x] != '.'))) {
	    return 0;
	}
    }

    return (x ? 1 : 0);		/* If the first character is ':', it's broken, too */
}

API_EXPORT(int) ap_can_exec(const struct stat *finfo)
{
    if (ap_user_id == finfo->st_uid)
	if (finfo->st_mode & S_IXUSR)
	    return 1;
    if (ap_group_id == finfo->st_gid)
	if (finfo->st_mode & S_IXGRP)
	    return 1;
    return ((finfo->st_mode & S_IXOTH) != 0);
}

API_EXPORT(int) ap_ind(const char *s, char c)
{
    register int x;

    for (x = 0; s[x]; x++)
	if (s[x] == c)
	    return x;

    return -1;
}

API_EXPORT(int) ap_rind(const char *s, char c)
{
    register int x;

    for (x = strlen(s) - 1; x != -1; x--)
	if (s[x] == c)
	    return x;

    return -1;
}

API_EXPORT(void) ap_str_tolower(char *str)
{
    while (*str) {
	*str = ap_tolower(*str);
	++str;
    }
}

API_EXPORT(uid_t) ap_uname2id(const char *name)
{
    struct passwd *ent;

    if (name[0] == '#')
	return (atoi(&name[1]));

    if (!(ent = getpwnam(name))) {
	fprintf(stderr, "%s: bad user name %s\n", ap_server_argv0, name);
	exit(1);
    }
    return (ent->pw_uid);
}

API_EXPORT(gid_t) ap_gname2id(const char *name)
{
    struct group *ent;

    if (name[0] == '#')
	return (atoi(&name[1]));

    if (!(ent = getgrnam(name))) {
	fprintf(stderr, "%s: bad group name %s\n", ap_server_argv0, name);
	exit(1);
    }
    return (ent->gr_gid);
}


/*
 * Parses a host of the form <address>[:port]
 * :port is permitted if 'port' is not NULL
 */
API_EXPORT(unsigned long) ap_get_virthost_addr(char *w, unsigned short *ports)
{
    struct hostent *hep;
    unsigned long my_addr;
    char *p;

    p = strchr(w, ':');
    if (ports != NULL) {
	*ports = 0;
	if (p != NULL && strcmp(p + 1, "*") != 0)
	    *ports = atoi(p + 1);
    }

    if (p != NULL)
	*p = '\0';
    if (strcmp(w, "*") == 0) {
	if (p != NULL)
	    *p = ':';
	return htonl(INADDR_ANY);
    }

    my_addr = ap_inet_addr((char *)w);
    if (my_addr != INADDR_NONE) {
	if (p != NULL)
	    *p = ':';
	return my_addr;
    }

    hep = gethostbyname(w);

    if ((!hep) || (hep->h_addrtype != AF_INET || !hep->h_addr_list[0])) {
	fprintf(stderr, "Cannot resolve host name %s --- exiting!\n", w);
	exit(1);
    }

    if (hep->h_addr_list[1]) {
	fprintf(stderr, "Host %s has multiple addresses ---\n", w);
	fprintf(stderr, "you must choose one explicitly for use as\n");
	fprintf(stderr, "a virtual host.  Exiting!!!\n");
	exit(1);
    }

    if (p != NULL)
	*p = ':';

    return ((struct in_addr *) (hep->h_addr))->s_addr;
}


static char *find_fqdn(pool *a, struct hostent *p)
{
    int x;

    if (!strchr(p->h_name, '.')) {
        if (p->h_aliases) {
        	for (x = 0; p->h_aliases[x]; ++x) {
                if (p->h_aliases[x] && strchr(p->h_aliases[x], '.') &&
            		(!strncasecmp(p->h_aliases[x], p->h_name, strlen(p->h_name))))
		            return ap_pstrdup(a, p->h_aliases[x]);
            }
	    }
	    return NULL;
    }
    return ap_pstrdup(a, (void *) p->h_name);
}

API_EXPORT(char *) ap_get_local_host(pool *a)
{
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif
    char str[MAXHOSTNAMELEN];
    char *server_hostname = NULL;
    struct hostent *p;

    if (gethostname(str, sizeof(str) - 1) != 0) {
	ap_log_error(APLOG_MARK, APLOG_WARNING, NULL,
	             "%s: gethostname() failed to determine ServerName\n",
                     ap_server_argv0);
    }
    else 
    {
        str[sizeof(str) - 1] = '\0';
        if ((!(p = gethostbyname(str))) 
            || (!(server_hostname = find_fqdn(a, p)))) {
           if (p == NULL || p->h_addr_list == NULL)
              server_hostname=NULL;
           else {
              /* Recovery - return the default servername by IP: */
              if (p->h_addr_list[0]) {
		      ap_snprintf(str, sizeof(str), "%pA", p->h_addr_list[0]);
		      server_hostname = ap_pstrdup(a, str);
                    /* We will drop through to report the IP-named server */
	      }
	   }
        }
	else
            /* Since we found a fqdn, return it with no logged message. */
            return server_hostname;
    }

    /* If we don't have an fqdn or IP, fall back to the loopback addr */
    if (!server_hostname) 
        server_hostname = ap_pstrdup(a, "127.0.0.1");
    
    ap_log_error(APLOG_MARK, APLOG_ALERT|APLOG_NOERRNO, NULL,
	         "%s: Could not determine the server's fully qualified "
                 "domain name, using %s for ServerName",
                 ap_server_argv0, server_hostname);
    
    return server_hostname;
}

/* simple 'pool' alloc()ing glue to ap_base64.c
 */
API_EXPORT(char *) ap_pbase64decode(pool *p, const char *bufcoded)
{
    char *decoded;
    int l;

    decoded = (char *) ap_palloc(p, 1 + ap_base64decode_len(bufcoded));
    l = ap_base64decode(decoded, bufcoded);
    decoded[l] = '\0'; /* make binary sequence into string */

    return decoded;
}

API_EXPORT(char *) ap_pbase64encode(pool *p, char *string) 
{ 
    char *encoded;
    int l = strlen(string);

    encoded = (char *) ap_palloc(p, 1 + ap_base64encode_len(l));
    l = ap_base64encode(encoded, string, l);
    encoded[l] = '\0'; /* make binary sequence into string */

    return encoded;
}

/* deprecated names for the above two functions, here for compatibility
 */
API_EXPORT(char *) ap_uudecode(pool *p, const char *bufcoded)
{
    return ap_pbase64decode(p, bufcoded);
}

API_EXPORT(char *) ap_uuencode(pool *p, char *string) 
{ 
    return ap_pbase64encode(p, string);
}


/* we want to downcase the type/subtype for comparison purposes
 * but nothing else because ;parameter=foo values are case sensitive.
 * XXX: in truth we want to downcase parameter names... but really,
 * apache has never handled parameters and such correctly.  You
 * also need to compress spaces and such to be able to compare
 * properly. -djg
 */
API_EXPORT(void) ap_content_type_tolower(char *str)
{
    char *semi;

    semi = strchr(str, ';');
    if (semi) {
	*semi = '\0';
    }
    while (*str) {
	*str = ap_tolower(*str);
	++str;
    }
    if (semi) {
	*semi = ';';
    }
}

/*
 * Given a string, replace any bare " with \" .
 */
API_EXPORT(char *) ap_escape_quotes (pool *p, const char *instring)
{
    int newlen = 0;
    const char *inchr = instring;
    char *outchr, *outstring;

    /*
     * Look through the input string, jogging the length of the output
     * string up by an extra byte each time we find an unescaped ".
     */
    while (*inchr != '\0') {
	newlen++;
        if (*inchr == '"') {
	    newlen++;
	}
	/*
	 * If we find a slosh, and it's not the last byte in the string,
	 * it's escaping something - advance past both bytes.
	 */
	if ((*inchr == '\\') && (inchr[1] != '\0')) {
	    inchr++;
	    newlen++;
	}
	inchr++;
    }
    outstring = ap_palloc(p, newlen + 1);
    inchr = instring;
    outchr = outstring;
    /*
     * Now copy the input string to the output string, inserting a slosh
     * in front of every " that doesn't already have one.
     */
    while (*inchr != '\0') {
	if ((*inchr == '\\') && (inchr[1] != '\0')) {
	    *outchr++ = *inchr++;
	    *outchr++ = *inchr++;
	}
	if (*inchr == '"') {
	    *outchr++ = '\\';
	}
	if (*inchr != '\0') {
	    *outchr++ = *inchr++;
	}
    }
    *outchr = '\0';
    return outstring;
}

/* dest = src with whitespace removed
 * length of dest assumed >= length of src
 */
API_EXPORT(void) ap_remove_spaces(char *dest, char *src)
{
    while (*src) {
        if (!ap_isspace(*src)) 
            *dest++ = *src;
        src++;
    }
    *dest = 0;
}
