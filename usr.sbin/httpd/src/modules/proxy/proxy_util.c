/* ====================================================================
 * Copyright (c) 1996-1999 The Apache Group.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * 4. The names "Apache Server" and "Apache Group" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY THE APACHE GROUP ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE APACHE GROUP OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Group and was originally based
 * on public domain software written at the National Center for
 * Supercomputing Applications, University of Illinois, Urbana-Champaign.
 * For more information on the Apache Group and the Apache HTTP server
 * project, please see <http://www.apache.org/>.
 *
 */

/* Utility routines for Apache proxy */
#include "mod_proxy.h"
#include "http_main.h"
#include "ap_md5.h"
#include "multithread.h"
#include "http_log.h"
#include "util_uri.h"
#include "util_date.h"	/* get ap_checkmask() decl. */

static int proxy_match_ipaddr(struct dirconn_entry *This, request_rec *r);
static int proxy_match_domainname(struct dirconn_entry *This, request_rec *r);
static int proxy_match_hostname(struct dirconn_entry *This, request_rec *r);
static int proxy_match_word(struct dirconn_entry *This, request_rec *r);

/* already called in the knowledge that the characters are hex digits */
int ap_proxy_hex2c(const char *x)
{
    int i, ch;

#ifndef CHARSET_EBCDIC
    ch = x[0];
    if (ap_isdigit(ch))
	i = ch - '0';
    else if (ap_isupper(ch))
	i = ch - ('A' - 10);
    else
	i = ch - ('a' - 10);
    i <<= 4;

    ch = x[1];
    if (ap_isdigit(ch))
	i += ch - '0';
    else if (ap_isupper(ch))
	i += ch - ('A' - 10);
    else
	i += ch - ('a' - 10);
    return i;
#else /*CHARSET_EBCDIC*/
    return (1 == sscanf(x, "%2x", &i)) ? os_toebcdic[i&0xFF] : 0;
#endif /*CHARSET_EBCDIC*/
}

void ap_proxy_c2hex(int ch, char *x)
{
#ifndef CHARSET_EBCDIC
    int i;

    x[0] = '%';
    i = (ch & 0xF0) >> 4;
    if (i >= 10)
	x[1] = ('A' - 10) + i;
    else
	x[1] = '0' + i;

    i = ch & 0x0F;
    if (i >= 10)
	x[2] = ('A' - 10) + i;
    else
	x[2] = '0' + i;
#else /*CHARSET_EBCDIC*/
    static const char ntoa[] = { "0123456789ABCDEF" };
    ch &= 0xFF;
    x[0] = '%';
    x[1] = ntoa[(os_toascii[ch]>>4)&0x0F];
    x[2] = ntoa[os_toascii[ch]&0x0F];
    x[3] = '\0';
#endif /*CHARSET_EBCDIC*/
}

/*
 * canonicalise a URL-encoded string
 */

/*
 * Convert a URL-encoded string to canonical form.
 * It decodes characters which need not be encoded,
 * and encodes those which must be encoded, and does not touch
 * those which must not be touched.
 */
char *
     ap_proxy_canonenc(pool *p, const char *x, int len, enum enctype t, int isenc)
{
    int i, j, ch;
    char *y;
    const char *allowed;	/* characters which should not be encoded */
    const char *reserved;	/* characters which much not be en/de-coded */

/* N.B. in addition to :@&=, this allows ';' in an http path
 * and '?' in an ftp path -- this may be revised
 * 
 * Also, it makes a '+' character in a search string reserved, as
 * it may be form-encoded. (Although RFC 1738 doesn't allow this -
 * it only permits ; / ? : @ = & as reserved chars.)
 */
    if (t == enc_path)
	allowed = "$-_.+!*'(),;:@&=";
    else if (t == enc_search)
	allowed = "$-_.!*'(),;:@&=";
    else if (t == enc_user)
	allowed = "$-_.+!*'(),;@&=";
    else if (t == enc_fpath)
	allowed = "$-_.+!*'(),?:@&=";
    else			/* if (t == enc_parm) */
	allowed = "$-_.+!*'(),?/:@&=";

    if (t == enc_path)
	reserved = "/";
    else if (t == enc_search)
	reserved = "+";
    else
	reserved = "";

    y = ap_palloc(p, 3 * len + 1);

    for (i = 0, j = 0; i < len; i++, j++) {
/* always handle '/' first */
	ch = x[i];
	if (strchr(reserved, ch)) {
	    y[j] = ch;
	    continue;
	}
/* decode it if not already done */
	if (isenc && ch == '%') {
	    if (!ap_isxdigit(x[i + 1]) || !ap_isxdigit(x[i + 2]))
		return NULL;
	    ch = ap_proxy_hex2c(&x[i + 1]);
	    i += 2;
	    if (ch != 0 && strchr(reserved, ch)) {	/* keep it encoded */
		ap_proxy_c2hex(ch, &y[j]);
		j += 2;
		continue;
	    }
	}
/* recode it, if necessary */
	if (!ap_isalnum(ch) && !strchr(allowed, ch)) {
	    ap_proxy_c2hex(ch, &y[j]);
	    j += 2;
	}
	else
	    y[j] = ch;
    }
    y[j] = '\0';
    return y;
}

/*
 * Parses network-location.
 *    urlp           on input the URL; on output the path, after the leading /
 *    user           NULL if no user/password permitted
 *    password       holder for password
 *    host           holder for host
 *    port           port number; only set if one is supplied.
 *
 * Returns an error string.
 */
char *
     ap_proxy_canon_netloc(pool *p, char **const urlp, char **userp,
			char **passwordp, char **hostp, int *port)
{
    int i;
    char *strp, *host, *url = *urlp;
    char *user = NULL, *password = NULL;

    if (url[0] != '/' || url[1] != '/')
	return "Malformed URL";
    host = url + 2;
    url = strchr(host, '/');
    if (url == NULL)
	url = "";
    else
	*(url++) = '\0';	/* skip seperating '/' */

    /* find _last_ '@' since it might occur in user/password part */
    strp = strrchr(host, '@');

    if (strp != NULL) {
	*strp = '\0';
	user = host;
	host = strp + 1;

/* find password */
	strp = strchr(user, ':');
	if (strp != NULL) {
	    *strp = '\0';
	    password = ap_proxy_canonenc(p, strp + 1, strlen(strp + 1), enc_user, 1);
	    if (password == NULL)
		return "Bad %-escape in URL (password)";
	}

	user = ap_proxy_canonenc(p, user, strlen(user), enc_user, 1);
	if (user == NULL)
	    return "Bad %-escape in URL (username)";
    }
    if (userp != NULL) {
	*userp = user;
    }
    if (passwordp != NULL) {
	*passwordp = password;
    }

    strp = strrchr(host, ':');
    if (strp != NULL) {
	*(strp++) = '\0';

	for (i = 0; strp[i] != '\0'; i++)
	    if (!ap_isdigit(strp[i]))
		break;

	/* if (i == 0) the no port was given; keep default */
	if (strp[i] != '\0') {
	    return "Bad port number in URL";
	} else if (i > 0) {
	    *port = atoi(strp);
	    if (*port > 65535)
		return "Port number in URL > 65535";
	}
    }
    ap_str_tolower(host);		/* DNS names are case-insensitive */
    if (*host == '\0')
	return "Missing host in URL";
/* check hostname syntax */
    for (i = 0; host[i] != '\0'; i++)
	if (!ap_isdigit(host[i]) && host[i] != '.')
	    break;
    /* must be an IP address */
#ifdef WIN32
    if (host[i] == '\0' && (inet_addr(host) == -1))
#else
    if (host[i] == '\0' && (ap_inet_addr(host) == -1 || inet_network(host) == -1))
#endif
    {
	return "Bad IP address in URL";
    }

/*    if (strchr(host,'.') == NULL && domain != NULL)
   host = pstrcat(p, host, domain, NULL);
 */
    *urlp = url;
    *hostp = host;

    return NULL;
}

static const char * const lwday[7] =
{"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

/*
 * If the date is a valid RFC 850 date or asctime() date, then it
 * is converted to the RFC 1123 format, otherwise it is not modified.
 * This routine is not very fast at doing conversions, as it uses
 * sscanf and sprintf. However, if the date is already correctly
 * formatted, then it exits very quickly.
 */
const char *
     ap_proxy_date_canon(pool *p, const char *x)
{
    int wk, mday, year, hour, min, sec, mon;
    char *q, month[4], zone[4], week[4];

    q = strchr(x, ',');
    /* check for RFC 850 date */
    if (q != NULL && q - x > 3 && q[1] == ' ') {
	*q = '\0';
	for (wk = 0; wk < 7; wk++)
	    if (strcmp(x, lwday[wk]) == 0)
		break;
	*q = ',';
	if (wk == 7)
	    return x;		/* not a valid date */
	if (q[4] != '-' || q[8] != '-' || q[11] != ' ' || q[14] != ':' ||
	    q[17] != ':' || strcmp(&q[20], " GMT") != 0)
	    return x;
	if (sscanf(q + 2, "%u-%3s-%u %u:%u:%u %3s", &mday, month, &year,
		   &hour, &min, &sec, zone) != 7)
	    return x;
	if (year < 70)
	    year += 2000;
	else
	    year += 1900;
    }
    else {
/* check for acstime() date */
	if (x[3] != ' ' || x[7] != ' ' || x[10] != ' ' || x[13] != ':' ||
	    x[16] != ':' || x[19] != ' ' || x[24] != '\0')
	    return x;
	if (sscanf(x, "%3s %3s %u %u:%u:%u %u", week, month, &mday, &hour,
		   &min, &sec, &year) != 7)
	    return x;
	for (wk = 0; wk < 7; wk++)
	    if (strcmp(week, ap_day_snames[wk]) == 0)
		break;
	if (wk == 7)
	    return x;
    }

/* check date */
    for (mon = 0; mon < 12; mon++)
	if (strcmp(month, ap_month_snames[mon]) == 0)
	    break;
    if (mon == 12)
	return x;

    q = ap_palloc(p, 30);
    ap_snprintf(q, 30, "%s, %.2d %s %d %.2d:%.2d:%.2d GMT", ap_day_snames[wk], mday,
		ap_month_snames[mon], year, hour, min, sec);
    return q;
}


/* NOTE: This routine is taken from http_protocol::getline()
 * because the old code found in the proxy module was too
 * difficult to understand and maintain.
 */
/* Get a line of protocol input, including any continuation lines
 * caused by MIME folding (or broken clients) if fold != 0, and place it
 * in the buffer s, of size n bytes, without the ending newline.
 *
 * Returns -1 on error, or the length of s.
 *
 * Note: Because bgets uses 1 char for newline and 1 char for NUL,
 *       the most we can get is (n - 2) actual characters if it
 *       was ended by a newline, or (n - 1) characters if the line
 *       length exceeded (n - 1).  So, if the result == (n - 1),
 *       then the actual input line exceeded the buffer length,
 *       and it would be a good idea for the caller to puke 400 or 414.
 */
static int proxy_getline(char *s, int n, BUFF *in, int fold)
{
    char *pos, next;
    int retval;
    int total = 0;

    pos = s;

    do {
        retval = ap_bgets(pos, n, in);     /* retval == -1 if error, 0 if EOF */

        if (retval <= 0)
            return ((retval < 0) && (total == 0)) ? -1 : total;

        /* retval is the number of characters read, not including NUL      */

        n -= retval;            /* Keep track of how much of s is full     */
        pos += (retval - 1);    /* and where s ends                        */
        total += retval;        /* and how long s has become               */

        if (*pos == '\n') {     /* Did we get a full line of input?        */
            *pos = '\0';
            --total;
            ++n;
        }
        else
            return total;       /* if not, input line exceeded buffer size */

        /* Continue appending if line folding is desired and
         * the last line was not empty and we have room in the buffer and
         * the next line begins with a continuation character.
         */
    } while (fold && (retval != 1) && (n > 1)
                  && (ap_blookc(&next, in) == 1)
                  && ((next == ' ') || (next == '\t')));

    return total;
}


/*
 * Reads headers from a buffer and returns an array of headers.
 * Returns NULL on file error
 * This routine tries to deal with too long lines and continuation lines.
 * @@@: XXX: FIXME: currently the headers are passed thru un-merged. 
 * Is that okay, or should they be collapsed where possible?
 */
table *ap_proxy_read_headers(request_rec *r, char *buffer, int size, BUFF *f)
{
    table *resp_hdrs;
    int len;
    char *value, *end;
    char field[MAX_STRING_LEN];

    resp_hdrs = ap_make_table(r->pool, 20);

    /*
     * Read header lines until we get the empty separator line, a read error,
     * the connection closes (EOF), or we timeout.
     */
    while ((len = proxy_getline(buffer, size, f, 1)) > 0) {
	
	if (!(value = strchr(buffer, ':'))) {     /* Find the colon separator */

	    /* Buggy MS IIS servers sometimes return invalid headers
	     * (an extra "HTTP/1.0 200, OK" line sprinkled in between
	     * the usual MIME headers). Try to deal with it in a sensible
	     * way, but log the fact.
	     * XXX: The mask check is buggy if we ever see an HTTP/1.10 */

	    if (!ap_checkmask(buffer, "HTTP/#.# ###*")) {
		/* Nope, it wasn't even an extra HTTP header. Give up. */
		return NULL;
	    }

	    ap_log_error(APLOG_MARK, APLOG_WARNING|APLOG_NOERRNO, r->server,
			 "proxy: Ignoring duplicate HTTP header "
			 "returned by %s (%s)", r->uri, r->method);
	    continue;
	}

        *value = '\0';
        ++value;
	/* XXX: RFC2068 defines only SP and HT as whitespace, this test is
	 * wrong... and so are many others probably.
	 */
        while (ap_isspace(*value))
            ++value;            /* Skip to start of value   */

	/* should strip trailing whitespace as well */
	for (end = &value[strlen(value)-1]; end > value && ap_isspace(*end); --end)
	    *end = '\0';

        ap_table_add(resp_hdrs, buffer, value);

	/* the header was too long; at the least we should skip extra data */
	if (len >= size - 1) { 
	    while ((len = proxy_getline(field, MAX_STRING_LEN, f, 1))
		    >= MAX_STRING_LEN - 1) {
		/* soak up the extra data */
	    }
	    if (len == 0) /* time to exit the larger loop as well */
		break;
	}
    }
    return resp_hdrs;
}

long int ap_proxy_send_fb(BUFF *f, request_rec *r, cache_req *c)
{
    int  ok;
    char buf[IOBUFSIZE];
    long total_bytes_rcvd;
    register int n, o, w;
    conn_rec *con = r->connection;
    int alternate_timeouts = 1;	/* 1 if we alternate between soft & hard timeouts */

    total_bytes_rcvd = 0;
    if (c != NULL)
        c->written = 0;

#ifdef CHARSET_EBCDIC
    /* The cache copy is ASCII, not EBCDIC, even for text/html) */
    ap_bsetflag(f, B_ASCII2EBCDIC|B_EBCDIC2ASCII, 0);
    if (c != NULL && c->fp != NULL)
	ap_bsetflag(c->fp, B_ASCII2EBCDIC|B_EBCDIC2ASCII, 0);
    ap_bsetflag(con->client, B_ASCII2EBCDIC|B_EBCDIC2ASCII, 0);
#endif

    /* Since we are reading from one buffer and writing to another,
     * it is unsafe to do a soft_timeout here, at least until the proxy
     * has its own timeout handler which can set both buffers to EOUT.
     */

    ap_kill_timeout(r);

#ifdef WIN32
    /* works fine under win32, so leave it */
    ap_hard_timeout("proxy send body", r);
    alternate_timeouts = 0;
#else
    /* CHECKME! Since hard_timeout won't work in unix on sends with partial
     * cache completion, we have to alternate between hard_timeout
     * for reads, and soft_timeout for send.  This is because we need
     * to get a return from ap_bwrite to be able to continue caching.
     * BUT, if we *can't* continue anyway, just use hard_timeout.
     * (Also, if no cache file is written, use hard timeouts)
     */

    if (c == NULL || c->len <= 0 || c->cache_completion == 1.0) {
        ap_hard_timeout("proxy send body", r);
        alternate_timeouts = 0;
    }
#endif

    /* Loop and ap_bread() while we can successfully read and write,
     * or (after the client aborted) while we can successfully
     * read and finish the configured cache_completion.
     */
    for (ok = 1; ok; ) {
        if (alternate_timeouts)
            ap_hard_timeout("proxy recv body from upstream server", r);

	/* Read block from server */
	n = ap_bread(f, buf, IOBUFSIZE);

        if (alternate_timeouts)
            ap_kill_timeout(r);
        else
            ap_reset_timeout(r);

	if (n == -1) {		/* input error */
	    if (c != NULL) {
		ap_log_rerror(APLOG_MARK, APLOG_ERR, c->req,
		    "proxy: error reading from %s", c->url);
		c = ap_proxy_cache_error(c);
	    }
	    break;
	}
	if (n == 0)
	    break;		/* EOF */
	o = 0;
	total_bytes_rcvd += n;

	/* Write to cache first. */
	/*@@@ XXX FIXME: Assuming that writing the cache file won't time out?!!? */
        if (c != NULL && c->fp != NULL) {
            if (ap_bwrite(c->fp, &buf[0], n) != n) {
                ap_log_rerror(APLOG_MARK, APLOG_ERR, c->req,
		    "proxy: error writing to %s", c->tempfile);
		c = ap_proxy_cache_error(c);
            } else {
                c->written += n;
            }
        }

	/* Write the block to the client, detect aborted transfers */
        while (!con->aborted && n > 0) {
            if (alternate_timeouts)
                ap_soft_timeout("proxy send body", r);

            w = ap_bwrite(con->client, &buf[o], n);

            if (alternate_timeouts)
                ap_kill_timeout(r);
            else
                ap_reset_timeout(r);

            if (w <= 0) {
                if (c != NULL && c->fp != NULL) {
                    /* when a send failure occurs, we need to decide
                     * whether to continue loading and caching the
                     * document, or to abort the whole thing
                     */
                    ok = (c->len > 0) &&
                         (c->cache_completion > 0) &&
                         (c->len * c->cache_completion < total_bytes_rcvd);

                    if (! ok) {
                        ap_pclosef(c->req->pool, c->fp->fd);
                        c->fp = NULL;
                        unlink(c->tempfile);
			c = NULL;
                    }
                }
                con->aborted = 1;
                break;
            }
            n -= w;
            o += w;
        } /* while client alive and more data to send */
    } /* loop and ap_bread while "ok" */

    if (!con->aborted)
	ap_bflush(con->client);

    ap_kill_timeout(r);
    return total_bytes_rcvd;
}

/*
 * Sends response line and headers.  Uses the client fd and the 
 * headers_out array from the passed request_rec to talk to the client
 * and to properly set the headers it sends for things such as logging.
 * 
 * A timeout should be set before calling this routine.
 */
void ap_proxy_send_headers(request_rec *r, const char *respline, table *t)
{
    int i;
    BUFF *fp = r->connection->client;
    table_entry *elts = (table_entry *) ap_table_elts(t)->elts;

    ap_bvputs(fp, respline, CRLF, NULL);

    for (i = 0; i < ap_table_elts(t)->nelts; ++i) {
	if (elts[i].key != NULL) {
	    ap_bvputs(fp, elts[i].key, ": ", elts[i].val, CRLF, NULL);
	    ap_table_addn(r->headers_out, elts[i].key, elts[i].val);
	}
    }

    ap_bputs(CRLF, fp);
}


/*
 * list is a comma-separated list of case-insensitive tokens, with
 * optional whitespace around the tokens.
 * The return returns 1 if the token val is found in the list, or 0
 * otherwise.
 */
int ap_proxy_liststr(const char *list, const char *val)
{
    int len, i;
    const char *p;

    len = strlen(val);

    while (list != NULL) {
	p = strchr(list, ',');
	if (p != NULL) {
	    i = p - list;
	    do
		p++;
	    while (ap_isspace(*p));
	}
	else
	    i = strlen(list);

	while (i > 0 && ap_isspace(list[i - 1]))
	    i--;
	if (i == len && strncasecmp(list, val, len) == 0)
	    return 1;
	list = p;
    }
    return 0;
}

#ifdef CASE_BLIND_FILESYSTEM

/*
 * On some platforms, the file system is NOT case sensitive. So, a == A
 * need to map to smaller set of characters
 */
void ap_proxy_hash(const char *it, char *val, int ndepth, int nlength)
{
    AP_MD5_CTX context;
    unsigned char digest[16];
    char tmp[26];
    int i, k, d;
    unsigned int x;
    static const char enc_table[32] = "abcdefghijklmnopqrstuvwxyz012345";

    ap_MD5Init(&context);
    ap_MD5Update(&context, (const unsigned char *) it, strlen(it));
    ap_MD5Final(digest, &context);

/* encode 128 bits as 26 characters, using a modified uuencoding */
/* the encoding is 5 bytes -> 8 characters
 * i.e. 128 bits is 3 x 5 bytes + 1 byte -> 3 * 8 characters + 2 characters
 */
    for (i = 0, k = 0; i < 15; i += 5) {
	x = (digest[i] << 24) | (digest[i + 1] << 16) | (digest[i + 2] << 8) | digest[i + 3];
	tmp[k++] = enc_table[x >> 27];
	tmp[k++] = enc_table[(x >> 22) & 0x1f];
	tmp[k++] = enc_table[(x >> 17) & 0x1f];
	tmp[k++] = enc_table[(x >> 12) & 0x1f];
	tmp[k++] = enc_table[(x >> 7) & 0x1f];
	tmp[k++] = enc_table[(x >> 2) & 0x1f];
	x = ((x & 0x3) << 8) | digest[i + 4];
	tmp[k++] = enc_table[x >> 5];
	tmp[k++] = enc_table[x & 0x1f];
    }
/* one byte left */
    x = digest[15];
    tmp[k++] = enc_table[x >> 3];	/* use up 5 bits */
    tmp[k++] = enc_table[x & 0x7];
    /* now split into directory levels */

    for (i = k = d = 0; d < ndepth; ++d) {
	memcpy(&val[i], &tmp[k], nlength);
	k += nlength;
	val[i + nlength] = '/';
	i += nlength + 1;
    }
    memcpy(&val[i], &tmp[k], 26 - k);
    val[i + 26 - k] = '\0';
}

#else

void ap_proxy_hash(const char *it, char *val, int ndepth, int nlength)
{
    AP_MD5_CTX context;
    unsigned char digest[16];
    char tmp[22];
    int i, k, d;
    unsigned int x;
#if defined(AIX) && defined(__ps2__)
    /* Believe it or not, AIX 1.x does not allow you to name a file '@',
     * so hack around it in the encoding. */
    static const char enc_table[64] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_%";
#else
    static const char enc_table[64] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_@";
#endif

    ap_MD5Init(&context);
    ap_MD5Update(&context, (const unsigned char *) it, strlen(it));
    ap_MD5Final(digest, &context);

/* encode 128 bits as 22 characters, using a modified uuencoding */
/* the encoding is 3 bytes -> 4 characters
 * i.e. 128 bits is 5 x 3 bytes + 1 byte -> 5 * 4 characters + 2 characters
 */
    for (i = 0, k = 0; i < 15; i += 3) {
	x = (digest[i] << 16) | (digest[i + 1] << 8) | digest[i + 2];
	tmp[k++] = enc_table[x >> 18];
	tmp[k++] = enc_table[(x >> 12) & 0x3f];
	tmp[k++] = enc_table[(x >> 6) & 0x3f];
	tmp[k++] = enc_table[x & 0x3f];
    }
/* one byte left */
    x = digest[15];
    tmp[k++] = enc_table[x >> 2];	/* use up 6 bits */
    tmp[k++] = enc_table[(x << 4) & 0x3f];
    /* now split into directory levels */

    for (i = k = d = 0; d < ndepth; ++d) {
	memcpy(&val[i], &tmp[k], nlength);
	k += nlength;
	val[i + nlength] = '/';
	i += nlength + 1;
    }
    memcpy(&val[i], &tmp[k], 22 - k);
    val[i + 22 - k] = '\0';
}

#endif /* CASE_BLIND_FILESYSTEM */

/*
 * Converts 8 hex digits to a time integer
 */
int ap_proxy_hex2sec(const char *x)
{
    int i, ch;
    unsigned int j;

    for (i = 0, j = 0; i < 8; i++) {
	ch = x[i];
	j <<= 4;
	if (ap_isdigit(ch))
	    j |= ch - '0';
	else if (ap_isupper(ch))
	    j |= ch - ('A' - 10);
	else
	    j |= ch - ('a' - 10);
    }
    if (j == 0xffffffff)
	return -1;		/* so that it works with 8-byte ints */
    else
	return j;
}

/*
 * Converts a time integer to 8 hex digits
 */
void ap_proxy_sec2hex(int t, char *y)
{
    int i, ch;
    unsigned int j = t;

    for (i = 7; i >= 0; i--) {
	ch = j & 0xF;
	j >>= 4;
	if (ch >= 10)
	    y[i] = ch + ('A' - 10);
	else
	    y[i] = ch + '0';
    }
    y[8] = '\0';
}


cache_req *ap_proxy_cache_error(cache_req *c)
{
    if (c != NULL) {
	if (c->fp != NULL) {
	    ap_pclosef(c->req->pool, c->fp->fd);
	    c->fp = NULL;
	}
	if (c->tempfile) unlink(c->tempfile);
    }
    return NULL;
}

int ap_proxyerror(request_rec *r, int statuscode, const char *message)
{
    ap_table_setn(r->notes, "error-notes",
		  ap_pstrcat(r->pool, 
			     "The proxy server could not handle the request "
			     "<EM><A HREF=\"", r->uri, "\">",
			     r->method, "&nbsp;", r->uri, "</A></EM>.<P>\n"
			     "Reason: <STRONG>", message, "</STRONG>", NULL));

    /* Allow the "error-notes" string to be printed by ap_send_error_response() */
    ap_table_setn(r->notes, "verbose-error-to", ap_pstrdup(r->pool, "*"));

    r->status_line = ap_psprintf(r->pool, "%3.3u Proxy Error", statuscode);
    return statuscode;
}

/*
 * This routine returns its own error message
 */
const char *
     ap_proxy_host2addr(const char *host, struct hostent *reqhp)
{
    int i;
    struct hostent *hp;
    static APACHE_TLS struct hostent hpbuf;
    static APACHE_TLS u_long ipaddr;
    static APACHE_TLS char *charpbuf[2];

    for (i = 0; host[i] != '\0'; i++)
	if (!ap_isdigit(host[i]) && host[i] != '.')
	    break;

    if (host[i] != '\0') {
	hp = gethostbyname(host);
	if (hp == NULL)
	    return "Host not found";
    }
    else {
	ipaddr = ap_inet_addr(host);
	hp = gethostbyaddr((char *) &ipaddr, sizeof(u_long), AF_INET);
	if (hp == NULL) {
	    memset(&hpbuf, 0, sizeof(hpbuf));
	    hpbuf.h_name = 0;
	    hpbuf.h_addrtype = AF_INET;
	    hpbuf.h_length = sizeof(u_long);
	    hpbuf.h_addr_list = charpbuf;
	    hpbuf.h_addr_list[0] = (char *) &ipaddr;
	    hpbuf.h_addr_list[1] = 0;
	    hp = &hpbuf;
	}
    }
    *reqhp = *hp;
    return NULL;
}

static const char *
     proxy_get_host_of_request(request_rec *r)
{
    char *url, *user = NULL, *password = NULL, *err, *host;
    int port = -1;

    if (r->hostname != NULL)
	return r->hostname;

    /* Set url to the first char after "scheme://" */
    if ((url = strchr(r->uri, ':')) == NULL
	|| url[1] != '/' || url[2] != '/')
	return NULL;

    url = ap_pstrdup(r->pool, &url[1]);	/* make it point to "//", which is what proxy_canon_netloc expects */

    err = ap_proxy_canon_netloc(r->pool, &url, &user, &password, &host, &port);

    if (err != NULL)
	ap_log_rerror(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, r,
		     "%s", err);

    r->hostname = host;

    return host;		/* ought to return the port, too */
}

/* Return TRUE if addr represents an IP address (or an IP network address) */
int ap_proxy_is_ipaddr(struct dirconn_entry *This, pool *p)
{
    const char *addr = This->name;
    long ip_addr[4];
    int i, quads;
    long bits;

    /* if the address is given with an explicit netmask, use that */
    /* Due to a deficiency in ap_inet_addr(), it is impossible to parse */
    /* "partial" addresses (with less than 4 quads) correctly, i.e.  */
    /* 192.168.123 is parsed as 192.168.0.123, which is not what I want. */
    /* I therefore have to parse the IP address manually: */
    /*if (proxy_readmask(This->name, &This->addr.s_addr, &This->mask.s_addr) == 0) */
    /* addr and mask were set by proxy_readmask() */
    /*return 1; */

    /* Parse IP addr manually, optionally allowing */
    /* abbreviated net addresses like 192.168. */

    /* Iterate over up to 4 (dotted) quads. */
    for (quads = 0; quads < 4 && *addr != '\0'; ++quads) {
	char *tmp;

	if (*addr == '/' && quads > 0)	/* netmask starts here. */
	    break;

	if (!ap_isdigit(*addr))
	    return 0;		/* no digit at start of quad */

	ip_addr[quads] = strtol(addr, &tmp, 0);

	if (tmp == addr)	/* expected a digit, found something else */
	    return 0;

	if (ip_addr[quads] < 0 || ip_addr[quads] > 255) {
	    /* invalid octet */
	    return 0;
	}

	addr = tmp;

	if (*addr == '.' && quads != 3)
	    ++addr;		/* after the 4th quad, a dot would be illegal */
    }

    for (This->addr.s_addr = 0, i = 0; i < quads; ++i)
	This->addr.s_addr |= htonl(ip_addr[i] << (24 - 8 * i));

    if (addr[0] == '/' && ap_isdigit(addr[1])) {	/* net mask follows: */
	char *tmp;

	++addr;

	bits = strtol(addr, &tmp, 0);

	if (tmp == addr)	/* expected a digit, found something else */
	    return 0;

	addr = tmp;

	if (bits < 0 || bits > 32)	/* netmask must be between 0 and 32 */
	    return 0;

    }
    else {
	/* Determine (i.e., "guess") netmask by counting the */
	/* number of trailing .0's; reduce #quads appropriately */
	/* (so that 192.168.0.0 is equivalent to 192.168.)        */
	while (quads > 0 && ip_addr[quads - 1] == 0)
	    --quads;

	/* "IP Address should be given in dotted-quad form, optionally followed by a netmask (e.g., 192.168.111.0/24)"; */
	if (quads < 1)
	    return 0;

	/* every zero-byte counts as 8 zero-bits */
	bits = 8 * quads;

	if (bits != 32)		/* no warning for fully qualified IP address */
	    fprintf(stderr, "Warning: NetMask not supplied with IP-Addr; guessing: %s/%ld\n",
		    inet_ntoa(This->addr), bits);
    }

    This->mask.s_addr = htonl(INADDR_NONE << (32 - bits));

    if (*addr == '\0' && (This->addr.s_addr & ~This->mask.s_addr) != 0) {
	fprintf(stderr, "Warning: NetMask and IP-Addr disagree in %s/%ld\n",
		inet_ntoa(This->addr), bits);
	This->addr.s_addr &= This->mask.s_addr;
	fprintf(stderr, "         Set to %s/%ld\n",
		inet_ntoa(This->addr), bits);
    }

    if (*addr == '\0') {
	This->matcher = proxy_match_ipaddr;
	return 1;
    }
    else
	return (*addr == '\0');	/* okay iff we've parsed the whole string */
}

/* Return TRUE if addr represents an IP address (or an IP network address) */
static int proxy_match_ipaddr(struct dirconn_entry *This, request_rec *r)
{
    int i;
    int ip_addr[4];
    struct in_addr addr;
    struct in_addr *ip_list;
    char **ip_listptr;
    const char *found;
    const char *host = proxy_get_host_of_request(r);

    if (host == NULL)   /* oops! */
       return 0;

    memset(&addr, '\0', sizeof addr);
    memset(ip_addr, '\0', sizeof ip_addr);

    if (4 == sscanf(host, "%d.%d.%d.%d", &ip_addr[0], &ip_addr[1], &ip_addr[2], &ip_addr[3])) {
	for (addr.s_addr = 0, i = 0; i < 4; ++i)
	    addr.s_addr |= htonl(ip_addr[i] << (24 - 8 * i));

	if (This->addr.s_addr == (addr.s_addr & This->mask.s_addr)) {
#if DEBUGGING
	    fprintf(stderr, "1)IP-Match: %s[%s] <-> ", host, inet_ntoa(addr));
	    fprintf(stderr, "%s/", inet_ntoa(This->addr));
	    fprintf(stderr, "%s\n", inet_ntoa(This->mask));
#endif
	    return 1;
	}
#if DEBUGGING
	else {
	    fprintf(stderr, "1)IP-NoMatch: %s[%s] <-> ", host, inet_ntoa(addr));
	    fprintf(stderr, "%s/", inet_ntoa(This->addr));
	    fprintf(stderr, "%s\n", inet_ntoa(This->mask));
	}
#endif
    }
    else {
	struct hostent the_host;

	memset(&the_host, '\0', sizeof the_host);
	found = ap_proxy_host2addr(host, &the_host);

	if (found != NULL) {
#if DEBUGGING
	    fprintf(stderr, "2)IP-NoMatch: hostname=%s msg=%s\n", host, found);
#endif
	    return 0;
	}

	if (the_host.h_name != NULL)
	    found = the_host.h_name;
	else
	    found = host;

	/* Try to deal with multiple IP addr's for a host */
	for (ip_listptr = the_host.h_addr_list; *ip_listptr; ++ip_listptr) {
	    ip_list = (struct in_addr *) *ip_listptr;
	    if (This->addr.s_addr == (ip_list->s_addr & This->mask.s_addr)) {
#if DEBUGGING
		fprintf(stderr, "3)IP-Match: %s[%s] <-> ", found, inet_ntoa(*ip_list));
		fprintf(stderr, "%s/", inet_ntoa(This->addr));
		fprintf(stderr, "%s\n", inet_ntoa(This->mask));
#endif
		return 1;
	    }
#if DEBUGGING
	    else {
		fprintf(stderr, "3)IP-NoMatch: %s[%s] <-> ", found, inet_ntoa(*ip_list));
		fprintf(stderr, "%s/", inet_ntoa(This->addr));
		fprintf(stderr, "%s\n", inet_ntoa(This->mask));
	    }
#endif
	}
    }

    return 0;
}

/* Return TRUE if addr represents a domain name */
int ap_proxy_is_domainname(struct dirconn_entry *This, pool *p)
{
    char *addr = This->name;
    int i;

    /* Domain name must start with a '.' */
    if (addr[0] != '.')
	return 0;

    /* rfc1035 says DNS names must consist of "[-a-zA-Z0-9]" and '.' */
    for (i = 0; ap_isalnum(addr[i]) || addr[i] == '-' || addr[i] == '.'; ++i)
	continue;

#if 0
    if (addr[i] == ':') {
	fprintf(stderr, "@@@@ handle optional port in proxy_is_domainname()\n");
	/* @@@@ handle optional port */
    }
#endif

    if (addr[i] != '\0')
	return 0;

    /* Strip trailing dots */
    for (i = strlen(addr) - 1; i > 0 && addr[i] == '.'; --i)
	addr[i] = '\0';

    This->matcher = proxy_match_domainname;
    return 1;
}

/* Return TRUE if host "host" is in domain "domain" */
static int proxy_match_domainname(struct dirconn_entry *This, request_rec *r)
{
    const char *host = proxy_get_host_of_request(r);
    int d_len = strlen(This->name), h_len;

    if (host == NULL)		/* some error was logged already */
	return 0;

    h_len = strlen(host);

    /* @@@ do this within the setup? */
    /* Ignore trailing dots in domain comparison: */
    while (d_len > 0 && This->name[d_len - 1] == '.')
	--d_len;
    while (h_len > 0 && host[h_len - 1] == '.')
	--h_len;
    return h_len > d_len
	&& strncasecmp(&host[h_len - d_len], This->name, d_len) == 0;
}

/* Return TRUE if addr represents a host name */
int ap_proxy_is_hostname(struct dirconn_entry *This, pool *p)
{
    struct hostent host;
    char *addr = This->name;
    int i;

    /* Host names must not start with a '.' */
    if (addr[0] == '.')
	return 0;

    /* rfc1035 says DNS names must consist of "[-a-zA-Z0-9]" and '.' */
    for (i = 0; ap_isalnum(addr[i]) || addr[i] == '-' || addr[i] == '.'; ++i);

#if 0
    if (addr[i] == ':') {
	fprintf(stderr, "@@@@ handle optional port in proxy_is_hostname()\n");
	/* @@@@ handle optional port */
    }
#endif

    if (addr[i] != '\0' || ap_proxy_host2addr(addr, &host) != NULL)
	return 0;

    This->hostentry = ap_pduphostent (p, &host);

    /* Strip trailing dots */
    for (i = strlen(addr) - 1; i > 0 && addr[i] == '.'; --i)
	addr[i] = '\0';

    This->matcher = proxy_match_hostname;
    return 1;
}

/* Return TRUE if host "host" is equal to host2 "host2" */
static int proxy_match_hostname(struct dirconn_entry *This, request_rec *r)
{
    char *host = This->name;
    const char *host2 = proxy_get_host_of_request(r);
    int h2_len;
    int h1_len;

    if (host == NULL || host2 == NULL)
       return 0; /* oops! */

    h2_len = strlen(host2);
    h1_len = strlen(host);

#if 0
    unsigned long *ip_list;

    /* Try to deal with multiple IP addr's for a host */
    for (ip_list = *This->hostentry->h_addr_list; *ip_list != 0UL; ++ip_list)
	if (*ip_list == ? ? ? ? ? ? ? ? ? ? ? ? ?)
	    return 1;
#endif

    /* Ignore trailing dots in host2 comparison: */
    while (h2_len > 0 && host2[h2_len - 1] == '.')
	--h2_len;
    while (h1_len > 0 && host[h1_len - 1] == '.')
	--h1_len;
    return h1_len == h2_len
	&& strncasecmp(host, host2, h1_len) == 0;
}

/* Return TRUE if addr is to be matched as a word */
int ap_proxy_is_word(struct dirconn_entry *This, pool *p)
{
    This->matcher = proxy_match_word;
    return 1;
}

/* Return TRUE if string "str2" occurs literally in "str1" */
static int proxy_match_word(struct dirconn_entry *This, request_rec *r)
{
    const char *host = proxy_get_host_of_request(r);
    return host != NULL && strstr(host, This->name) != NULL;
}

int ap_proxy_doconnect(int sock, struct sockaddr_in *addr, request_rec *r)
{
    int i;

    ap_hard_timeout("proxy connect", r);
    do {
	i = connect(sock, (struct sockaddr *) addr, sizeof(struct sockaddr_in));
#ifdef WIN32
	if (i == SOCKET_ERROR)
	    errno = WSAGetLastError();
#endif /* WIN32 */
    } while (i == -1 && errno == EINTR);
    if (i == -1) {
	ap_log_rerror(APLOG_MARK, APLOG_ERR, r,
		     "proxy connect to %s port %d failed",
		     inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
    }
    ap_kill_timeout(r);

    return i;
}

/* This function is called by ap_table_do() for all header lines */
/* (from proxy_http.c and proxy_ftp.c) */
/* It is passed a table_do_args struct pointer and a MIME field and value pair */
int ap_proxy_send_hdr_line(void *p, const char *key, const char *value)
{
    struct tbl_do_args *parm = (struct tbl_do_args *)p;

    if (key == NULL || value == NULL || value[0] == '\0')
	return 1;
    if (!parm->req->assbackwards)
	ap_rvputs(parm->req, key, ": ", value, CRLF, NULL);
    if (parm->cache != NULL && parm->cache->fp != NULL &&
	ap_bvputs(parm->cache->fp, key, ": ", value, CRLF, NULL) == -1) {
	    ap_log_rerror(APLOG_MARK, APLOG_ERR, parm->cache->req,
		    "proxy: error writing header to %s", parm->cache->tempfile);
	    parm->cache = ap_proxy_cache_error(parm->cache);
    }
    return 1; /* tell ap_table_do() to continue calling us for more headers */
}

/* send a text line to one or two BUFF's; return line length */
unsigned ap_proxy_bputs2(const char *data, BUFF *client, cache_req *cache)
{
    unsigned len = ap_bputs(data, client);
    if (cache != NULL && cache->fp != NULL)
	ap_bputs(data, cache->fp);
    return len;
}

