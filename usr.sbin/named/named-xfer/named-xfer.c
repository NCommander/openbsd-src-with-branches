/*	$NetBSD: named-xfer.c,v 1.2 1996/03/21 18:24:16 jtc Exp $	*/

/*
 * The original version of xfer by Kevin Dunlap.
 * Completed and integrated with named by David Waitzman
 *	(dwaitzman@bbn.com) 3/14/88.
 * Modified by M. Karels and O. Kure 10-88.
 * Modified extensively since then by just about everybody.
 */

/*
 * ++Copyright++ 1988, 1990
 * -
 * Copyright (c) 1988, 1990
 *    The Regents of the University of California.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

#if !defined(lint) && !defined(SABER)
char copyright[] =
"@(#) Copyright (c) 1988, 1990 The Regents of the University of California.\n\
 portions Copyright (c) 1993 Digital Equipment Corporation\n\
 All rights reserved.\n";
#endif /* not lint */

#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)named-xfer.c	4.18 (Berkeley) 3/7/91";
static char rcsid[] = "$Id: named-xfer.c,v 8.10 1995/12/06 20:34:38 vixie Exp ";
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <netinet/in.h>
#if defined(__osf__)
# include <sys/mbuf.h>
# include <net/route.h>
#endif
#if defined(_AIX)
# include <sys/time.h>
# define TIME_H_INCLUDED
#endif
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <errno.h>
#include <resolv.h>
#include <stdio.h>
#include <syslog.h>
#if !defined(SVR4) || !defined(sun)
# include <math.h>
#endif
#include <ctype.h>
#include <signal.h>

#define MAIN_PROGRAM
#include "named.h"
#undef MAIN_PROGRAM

#ifndef LOG_PERROR
# define LOG_PERROR 0
#endif

static	struct zoneinfo	zone;		/* zone information */

static	char		ddtfilename[] = _PATH_TMPXFER,
			*ddtfile = ddtfilename,
			*tmpname,
			*domain;		/* domain being xfered */

static	int		quiet = 0,
			read_interrupted = 0,
			curclass,
			domain_len;		/* strlen(domain) */

static	FILE		*fp = NULL,
			*dbfp = NULL;

static	char		*ProgName;

static	void		usage __P((const char *));
static	int		getzone __P((struct zoneinfo *, u_int32_t, int)),
			print_output __P((u_char *, int, u_char *)),
			netread __P((int, char *, int, int));
static	SIG_FN		read_alarm __P(());
static	const char	*soa_zinfo __P((struct zoneinfo *, u_char *, u_char*));

extern char *optarg;
extern int optind, getopt();

int
main(argc, argv)
	int argc;
	char *argv[];
{
	register struct zoneinfo *zp;
	register struct hostent *hp;
 	char *dbfile = NULL, *tracefile = NULL, *tm = NULL;
	int dbfd, ddtd, result, c, fd, closed = 0;
	u_int32_t serial_no = 0;
	u_int16_t port = htons(NAMESERVER_PORT);
	struct stat statbuf;
#ifdef STUBS
	int stub_only = 0;
#endif
#ifdef GEN_AXFR
	int class = C_IN;
#endif

	if (ProgName = strrchr(argv[0], '/'))
		ProgName++;
	else
		ProgName = argv[0];

	(void) umask(022);

	/* this is a hack; closing everything in the parent is hard. */
	for (fd = getdtablesize()-1;  fd > STDERR_FILENO;  fd--)
		closed += (close(fd) == 0);

#ifdef RENICE
	nice(-40);	/* this is the recommended procedure to        */
	nice(20);	/*   reset the priority of the current process */
	nice(0);	/*   to "normal" (== 0) - see nice(3)          */
#endif

#ifdef LOG_DAEMON
	openlog(ProgName, LOG_PID|LOG_CONS|LOG_PERROR, LOGFAC);
#else
	openlog(ProgName, LOG_PID);
#endif
#ifdef STUBS
	while ((c = getopt(argc, argv, "C:d:l:s:t:z:f:p:P:qS")) != EOF)
#else
	while ((c = getopt(argc, argv, "C:d:l:s:t:z:f:p:P:q")) != EOF)
#endif
	    switch (c) {
#ifdef GEN_AXFR
	        case 'C':
			class = get_class(optarg);
			break;
#endif
		case 'd':
#ifdef DEBUG
			debug = atoi(optarg);
#endif
			break;
		case 'l':
			ddtfile = (char *)malloc(strlen(optarg) +
						 sizeof(".XXXXXX") + 1);
			if (!ddtfile)
				panic(errno, "malloc(ddtfile)");
#ifdef SHORT_FNAMES
			filenamecpy(ddtfile, optarg);
#else
			(void) strcpy(ddtfile, optarg);
#endif /* SHORT_FNAMES */
			(void) strcat(ddtfile, ".XXXXXX");
			break;
		case 's':
			serial_no = strtoul(optarg, (char **)NULL, 10);
			break;
		case 't':
			tracefile = optarg;
			break;
		case 'z':		/* zone == domain */
			domain = optarg;
			domain_len = strlen(domain);
			while ((domain_len > 0) && 
					(domain[domain_len-1] == '.'))
				domain[--domain_len] = '\0';
			break;
		case 'f':
			dbfile = optarg;
			tmpname = (char *)malloc((unsigned)strlen(optarg) +
						 sizeof(".XXXXXX") + 1);
			if (!tmpname)
				panic(errno, "malloc(tmpname)");
#ifdef SHORT_FNAMES
			filenamecpy(tmpname, optarg);
#else
			(void) strcpy(tmpname, optarg);
#endif /* SHORT_FNAMES */
			break;
		case 'p':
			port = htons((u_int16_t)atoi(optarg));
			break;
		case 'P':
			port = (u_int16_t)atoi(optarg);
			break;
#ifdef STUBS
		case 'S':
			stub_only = 1;
			break;
#endif
		case 'q':
			quiet++;
			break;
		case '?':
		default:
			usage("unrecognized argument");
			/* NOTREACHED */
	    }

	if (!domain || !dbfile || optind >= argc) {
		if (!domain)
			usage("no domain");
		if (!dbfile)
			usage("no dbfile");
		if (optind >= argc)
			usage("not enough arguments");
		/* NOTREACHED */
	}
	if (stat(dbfile, &statbuf) != -1 &&
	    !S_ISREG(statbuf.st_mode) &&
	    !S_ISFIFO(statbuf.st_mode))
		usage("dbfile must be a regular file or FIFO");
	if (tracefile && (fp = fopen(tracefile, "w")) == NULL)
		perror(tracefile);
	(void) strcat(tmpname, ".XXXXXX");
	/* tmpname is now something like "/etc/named/named.bu.db.XXXXXX" */
	if ((dbfd = mkstemp(tmpname)) == -1) {
		perror(tmpname);
		if (!quiet)
			syslog(LOG_ERR, "can't make tmpfile (%s): %m\n",
			       tmpname);
		exit(XFER_FAIL);
	}
#if HAVE_FCHMOD
	if (fchmod(dbfd, 0644) == -1)
#else
	if (chmod(tmpname, 0644) == -1)
#endif
	{
		perror(tmpname);
		if (!quiet)
			syslog(LOG_ERR, "can't [f]chmod tmpfile (%s): %m\n",
			       tmpname);
		exit(XFER_FAIL);
	}
	if ((dbfp = fdopen(dbfd, "r+")) == NULL) {
		perror(tmpname);
		if (!quiet)
			syslog(LOG_ERR, "can't fdopen tmpfile (%s)", tmpname);
		exit(XFER_FAIL);
	}
#ifdef DEBUG
	if (debug) {
		/* ddtfile is now something like "/usr/tmp/xfer.ddt.XXXXXX" */
		if ((ddtd = mkstemp(ddtfile)) == -1) {
			perror(ddtfile);
			debug = 0;
		}
#if HAVE_FCHMOD
		else if (fchmod(ddtd, 0644) == -1)
#else
		else if (chmod(ddtfile, 0644) == -1)
#endif
		{
			perror(ddtfile);
			debug = 0;
		} else if ((ddt = fdopen(ddtd, "w")) == NULL) {
			perror(ddtfile);
			debug = 0;
		} else {
#ifdef HAVE_SETVBUF
			setvbuf(ddt, NULL, _IOLBF, BUFSIZ);
#else
			setlinebuf(ddt);
#endif
		}
	}
#endif
	/*
	 * Ignore many types of signals that named (assumed to be our parent)
	 * considers important- if not, the user controlling named with
	 * signals usually kills us.
	 */
	(void) signal(SIGHUP, SIG_IGN);
#ifdef SIGSYS
	(void) signal(SIGSYS, SIG_IGN);
#endif
#ifdef DEBUG
	if (debug == 0)
#endif
	{
		(void) signal(SIGINT, SIG_IGN);
		(void) signal(SIGQUIT, SIG_IGN);
	}
	(void) signal(SIGIOT, SIG_IGN);

#if defined(SIGUSR1) && defined(SIGUSR2)
	(void) signal(SIGUSR1, SIG_IGN);
	(void) signal(SIGUSR2, SIG_IGN);
#else	/* SIGUSR1&&SIGUSR2 */
	(void) signal(SIGEMT, SIG_IGN);
	(void) signal(SIGFPE, SIG_IGN);
#endif /* SIGUSR1&&SIGUSR2 */

	dprintf(1, (ddt,
		    "domain `%s'; file `%s'; serial %lu; closed %d\n",
		    domain, dbfile, (u_long)serial_no, closed));

	buildservicelist();
	buildprotolist();

	/* init zone data */
 
	zp = &zone;
#ifdef STUBS
	if (stub_only)
		zp->z_type = Z_STUB;
	else
#endif
		zp->z_type = Z_SECONDARY;
#ifdef GEN_AXFR
	zp->z_class = class;
#endif
	zp->z_origin = domain;
	zp->z_source = dbfile;
	zp->z_addrcnt = 0;
	dprintf(1, (ddt, "zone found (%d): \"%s\", source = %s\n",
		    zp->z_type,
		    (zp->z_origin[0] == '\0')
			? "."
		        : zp->z_origin,
		    zp->z_source));

	for (;  optind != argc;  optind++) {
		tm = argv[optind];
		if (!inet_aton(tm, &zp->z_addr[zp->z_addrcnt])) {
			hp = gethostbyname(tm);
			if (hp == NULL) {
				syslog(LOG_NOTICE,
				       "uninterpretable server (%s) for %s\n",
				       tm, zp->z_origin);
				continue;
			}
			bcopy(hp->h_addr,
			      (char *)&zp->z_addr[zp->z_addrcnt],
			      INADDRSZ);
			dprintf(1, (ddt, "Arg: \"%s\"\n", tm));
		}
		if (zp->z_addr[zp->z_addrcnt].s_addr == 0) {
			syslog(LOG_NOTICE,
			       "SOA query to 0.0.0.0 (%s) for %s",
			       tm, zp->z_origin);
			continue;
		}
		if (++zp->z_addrcnt >= NSMAX) {
			zp->z_addrcnt = NSMAX;
			dprintf(1, (ddt, "NSMAX reached\n"));
			break;
		}
	}
	dprintf(1, (ddt, "addrcnt = %d\n", zp->z_addrcnt));

	res_init();
	_res.options &= ~(RES_DEFNAMES | RES_DNSRCH | RES_RECURSE);
	result = getzone(zp, serial_no, port);
	(void) my_fclose(dbfp);
	switch (result) {

	case XFER_SUCCESS:			/* ok exit */
		if (rename(tmpname, dbfile) == -1) {
			perror("rename");
			if (!quiet)
			    syslog(LOG_ERR, "rename %s to %s: %m",
				   tmpname, dbfile);
			exit(XFER_FAIL);
		}
		exit(XFER_SUCCESS);

	case XFER_UPTODATE:		/* the zone was already uptodate */
		(void) unlink(tmpname);
		exit(XFER_UPTODATE);

	case XFER_TIMEOUT:
#ifdef DEBUG
		if (!debug)
#endif
		    (void) unlink(tmpname);
		exit(XFER_TIMEOUT);	/* servers not reachable exit */

	case XFER_FAIL:
	default:
#ifdef DEBUG
		if (!debug)
#endif
		    (void) unlink(tmpname);
		exit(XFER_FAIL);	/* yuck exit */
	}
	/*NOTREACHED*/
}
 
static char *UsageText[] = {
	"\t-z zone_to_transfer\n",
	"\t-f db_file\n",
	"\t-s serial_no\n",
	"\t[-d debug_level]\n",
	"\t[-l debug_log_file]\n",
	"\t[-t trace_file]\n",
	"\t[-p port]\n",
#ifdef STUBS
	"\t[-S]\n",
#endif
#ifdef GEN_AXFR
	"\t[-C class]\n",
#endif
	"\tservers...\n",
	NULL
};

static void
usage(msg)
	const char *msg;
{
	char * const *line;

	fprintf(stderr, "Usage error: %s\n", msg);
	fprintf(stderr, "Usage: %s\n", ProgName);
	for (line = UsageText;  *line;  line++)
		fputs(*line, stderr);
	exit(XFER_FAIL);
}

#define DEF_DNAME	'\001'		/* '\0' means the root domain */
/* XXX: The following variables should probably all be "static" */
int	minimum_ttl = 0, got_soa = 0;
int	prev_comment = 0;		/* was previous record a comment? */
char	zone_top[MAXDNAME];		/* the top of the zone */
char	prev_origin[MAXDNAME];		/* from most recent $ORIGIN line */
char	prev_dname[MAXDNAME] = { DEF_DNAME };	/* from previous record */
char	prev_ns_dname[MAXDNAME] = { DEF_DNAME }; /* from most recent NS record */

static int
getzone(zp, serial_no, port)
	struct zoneinfo *zp;
	u_int32_t serial_no;
	int port;
{
	HEADER *hp;
	u_int16_t len;
	u_int32_t serial;
	int s, n, l, nscnt, soacnt, error = 0;
	u_int cnt;
 	u_char *cp, *nmp, *eom, *tmp ;
	u_char *buf = NULL;
	u_int bufsize;
	char name[MAXDNAME], name2[MAXDNAME];
	struct sockaddr_in sin;
	struct zoneinfo zp_start, zp_finish;
#ifdef POSIX_SIGNALS
	struct sigaction sv, osv;
#else
	struct sigvec sv, osv;
#endif
	int qdcount, ancount, aucount, class, type;
	const char *badsoa_msg = "Nil";

#ifdef DEBUG
	if (debug) {
		(void)fprintf(ddt,"getzone() %s ", zp->z_origin);
		switch (zp->z_type) {
		case Z_STUB:
			fprintf(ddt,"stub\n");
			break;
		case Z_SECONDARY:
			fprintf(ddt,"secondary\n");
			break;
		default:
			fprintf(ddt,"unknown type\n");
		}
	}
#endif
#ifdef POSIX_SIGNALS
	bzero((char *)&sv, sizeof sv);
	sv.sa_handler = (SIG_FN (*)()) read_alarm;
	/* SA_ONSTACK isn't recommended for strict POSIX code */
	/* is it absolutely necessary? */
	/* sv.sa_flags = SA_ONSTACK; */
	sigfillset(&sv.sa_mask);
	(void) sigaction(SIGALRM, &sv, &osv);
#else
	bzero((char *)&sv, sizeof sv);
	sv.sv_handler = read_alarm;
	sv.sv_mask = ~0;
	(void) sigvec(SIGALRM, &sv, &osv);
#endif

	strcpy(zone_top, zp->z_origin);
	if ((l = strlen(zone_top)) != 0 && zone_top[l - 1] == '.')
		zone_top[l - 1] = '\0';
	strcpy(prev_origin, zone_top);

	for (cnt = 0; cnt < zp->z_addrcnt; cnt++) {
#ifdef GEN_AXFR
		curclass = zp->z_class;
#else
		curclass = C_IN;
#endif
		error = 0;
		if (buf == NULL) {
			if ((buf = (u_char *)malloc(2 * PACKETSZ)) == NULL) {
				syslog(LOG_INFO, "malloc(%u) failed",
				       2 * PACKETSZ);
				error++;
				break;
			}
			bufsize = 2 * PACKETSZ;
		}
		bzero((char *)&sin, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port = (u_int16_t)port;
		sin.sin_addr = zp->z_addr[cnt];
		if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			syslog(LOG_INFO, "socket: %m");
			error++;
			break;
		}	
		dprintf(2, (ddt, "connecting to server #%d [%s].%d\n",
			    cnt+1, inet_ntoa(sin.sin_addr),
			    ntohs(sin.sin_port)));
		if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
			if (!quiet)
				syslog(LOG_INFO,
				       "connect(%s) for zone %s failed: %m",
				       inet_ntoa(sin.sin_addr), zp->z_origin);
			error++;
			(void) my_close(s);
			continue;
		}	
#ifndef GEN_AXFR
 tryagain:
#endif
		n = res_mkquery(QUERY, zp->z_origin, curclass,
				T_SOA, NULL, 0, NULL, buf, bufsize);
		if (n < 0) {
			if (!quiet)
				syslog(LOG_INFO,
				       "zone %s: res_mkquery T_SOA failed",
				       zp->z_origin);
			(void) my_close(s);
#ifdef POSIX_SIGNALS
			(void) sigaction(SIGALRM, &osv, (struct sigaction *)0);
#else
			(void) sigvec(SIGALRM, &osv, (struct sigvec *)0);
#endif
			return (XFER_FAIL);
		}
		/*
		 * Send length & message for SOA query
		 */
		if (writemsg(s, buf, n) < 0) {
			syslog(LOG_INFO, "writemsg: %m");
			error++;
			(void) my_close(s);
			continue;	
		}
		/*
		 * Get out your butterfly net and catch the SOA
		 */
		if (netread(s, (char *)buf, INT16SZ, XFER_TIMER) < 0) {
			error++;
			(void) my_close(s);
			continue;
		}
		if ((len = _getshort(buf)) == 0) {
			(void) my_close(s);
			continue;
		}
		if (len > bufsize) {
			if ((buf = (u_char *)realloc(buf, len)) == NULL) {
				syslog(LOG_INFO,
		       "malloc(%u) failed for SOA from server [%s], zone %s\n",
				       len,
				       inet_ntoa(sin.sin_addr),
				       zp->z_origin);
				(void) my_close(s);
				continue;
			}
			bufsize = len;
		}
		if (netread(s, (char *)buf, len, XFER_TIMER) < 0) {
			error++;
			(void) my_close(s);
			continue;
		}
#ifdef DEBUG
		if (debug >= 3) {
			(void)fprintf(ddt,"len = %d\n", len);
			fp_nquery(buf, len, ddt);
		}
#endif
		hp = (HEADER *) buf;
		qdcount = ntohs(hp->qdcount);
		ancount = ntohs(hp->ancount);
		aucount = ntohs(hp->nscount);

		/*
		 * close socket if any of these apply:
		 *  1) rcode != NOERROR
		 *  2) not an authority response
		 *  3) not an answer to our question
		 *  4) both the number of answers and authority count < 1)
		 */
		if (hp->rcode != NOERROR || !hp->aa || qdcount != 1 ||
		    (ancount < 1 && aucount < 1)) {
#ifndef GEN_AXFR
			if (curclass == C_IN) {
				dprintf(1, (ddt, "SOA failed, trying C_HS\n"));
				curclass = C_HS;
				goto tryagain;
			}
#endif
			syslog(LOG_NOTICE,
       "[%s] %s for %s, SOA query got rcode %d, aa %d, ancount %d, aucount %d",
			       inet_ntoa(sin.sin_addr),
			       (hp->aa
				? (qdcount==1 ?"no SOA found" :"bad response")
				: "not authoritative"),
			       zp->z_origin[0] != '\0' ? zp->z_origin : ".",
			       hp->rcode, hp->aa, ancount, aucount);
			error++;
			(void) my_close(s);
			continue;
		}
		zp_start = *zp;
		if ((int)len < HFIXEDSZ + QFIXEDSZ) {
			badsoa_msg = "too short";
 badsoa:
			syslog(LOG_INFO,
			       "malformed SOA from [%s], zone %s: %s",
			       inet_ntoa(sin.sin_addr), zp->z_origin,
			       badsoa_msg);
			error++;
			(void) my_close(s);
			continue;
		}
		/*
		 * Step through response.
		 */
		tmp = buf + HFIXEDSZ;
		eom = buf + len;
		/* Query Section. */
		n = dn_expand(buf, eom, tmp, name2, sizeof name2);
		if (n < 0) {
			badsoa_msg = "qname error";
			goto badsoa;
		}
		tmp += n;
		GETSHORT(type, tmp);
		GETSHORT(class, tmp);
		if (class != curclass || type != T_SOA ||
		    strcasecmp(zp->z_origin, name2) != 0) {
			syslog(LOG_INFO,
			"wrong query in resp from [%s], zone %s: [%s %s %s]\n",
			       inet_ntoa(sin.sin_addr), zp->z_origin,
			       name2, p_class(class), p_type(type));
			error++;
			(void) my_close(s);
			continue;
		}
		/* ... Answer Section. */
		n = dn_expand(buf, eom, tmp, name2, sizeof name2);
		if (n < 0) {
			badsoa_msg = "aname error";
			goto badsoa;
		}
		tmp += n;
		if (strcasecmp(zp->z_origin, name2) != 0) {
			syslog(LOG_INFO,
		       "wrong answer in resp from [%s], zone %s: [%s %s %s]\n",
			       inet_ntoa(sin.sin_addr), zp->z_origin,
			       name2, p_class(class), p_type(type));
			error++;
			(void) my_close(s);
			continue;
		}
		badsoa_msg = soa_zinfo(&zp_start, tmp, eom);
		if (badsoa_msg)
			goto badsoa;
		if (SEQ_GT(zp_start.z_serial, serial_no) || !serial_no) {
			const char *l, *nl;
			dprintf(1, (ddt, "need update, serial %lu\n",
				    (u_long)zp_start.z_serial));
			hp = (HEADER *) buf;
			soacnt = 0;
			nscnt = 0;
			gettime(&tt);
			for (l = Version; l; l = nl) {
				size_t len;
				if ((nl = strchr(l, '\n')) != NULL) {
					len = nl - l;
					nl = nl + 1;
				} else {
					len = strlen(l);
					nl = NULL;
				}
				while (isspace((unsigned char) *l))
					l++;
				if (*l)
					fprintf(dbfp, "; BIND version %.*s\n",
						(int)len, l);
			}
			fprintf(dbfp, "; zone '%s'   last serial %lu\n",
				domain, (u_long)serial_no);
			fprintf(dbfp, "; from %s   at %s",
				inet_ntoa(sin.sin_addr),
				ctimel(tt.tv_sec));
			for (;;) {
				if ((soacnt == 0) || (zp->z_type == Z_STUB)) {
					int type;
#ifdef STUBS
					if (zp->z_type == Z_STUB) {
						if (!soacnt)
							type = T_SOA;
						else if (!nscnt)
							type = T_NS;
						else 
							type = T_SOA;
					} else
#endif
						type = T_AXFR;
					n = res_mkquery(QUERY, zp->z_origin,
							curclass, type,
							NULL, 0,
							NULL, buf, bufsize);
					if (n < 0) {
						if (!quiet) {
#ifdef STUBS
						    if (zp->z_type == Z_STUB)
							syslog(LOG_INFO,
							       (type == T_SOA)
					? "zone %s: res_mkquery T_SOA failed"
					: "zone %s: res_mkquery T_NS failed",
							       zp->z_origin);
						    else
#endif
							syslog(LOG_INFO,
					  "zone %s: res_mkquery T_AXFR failed",
							       zp->z_origin);
						}
						(void) my_close(s);
#ifdef POSIX_SIGNALS
						sigaction(SIGALRM, &osv,
							(struct sigaction *)0);
#else
						sigvec(SIGALRM, &osv,
						       (struct sigvec *)0);
#endif
						return (XFER_FAIL);
					}
					/*
					 * Send length & msg for zone transfer
					 */
					if (writemsg(s, buf, n) < 0) {
						syslog(LOG_INFO,
						       "writemsg: %m");
						error++;
						(void) my_close(s);
						break;	
					}
				}
				/*
				 * Receive length & response
				 */
				if (netread(s, (char *)buf, INT16SZ,
					    (soacnt == 0) ?300 :XFER_TIMER)
				    < 0) {
					error++;
					break;
				}
				if ((len = _getshort(buf)) == 0)
					break;
				eom = buf + len;
				if (netread(s, (char *)buf, len, XFER_TIMER)
				    < 0) {
					error++;
					break;
				}
#ifdef DEBUG
				if (debug >= 3) {
					(void)fprintf(ddt,"len = %d\n", len);
					fp_nquery(buf, len, ddt);
				}
				if (fp)
					fp_nquery(buf, len, fp);
#endif
				if (len < HFIXEDSZ) {
		badrec:
					error++;
					syslog(LOG_INFO,
				       "record too short from [%s], zone %s\n",
					       inet_ntoa(sin.sin_addr),
					       zp->z_origin);
					break;
				}
				cp = buf + HFIXEDSZ;
				if (hp->qdcount) {
					if ((n = dn_skipname(cp, eom)) == -1
					    || n + QFIXEDSZ >= eom - cp)
						goto badrec;
					cp += n + QFIXEDSZ;
				}
				nmp = cp;
				if ((n = dn_skipname(cp, eom)) == -1)
					goto badrec;
				tmp = cp + n;
#ifdef STUBS
			if (zp->z_type == Z_STUB) {
				ancount = ntohs(hp->ancount);
				for (cnt = 0 ; cnt < ancount ; cnt++) {

					n = print_output(buf, bufsize, cp);
					cp += n;
				}
				if (hp->nscount) {
					/* we should not get here */
					ancount = ntohs(hp->nscount);
					for (cnt = 0 ; cnt < ancount ; cnt++) {
					    n = print_output(buf, bufsize, cp);
						cp += n;
					}
				}
				ancount = ntohs(hp->arcount);
				for (cnt = 0 ; cnt < ancount ; cnt ++) {
					n = print_output(buf, bufsize, cp);
					cp += n;
				}
				if (cp != eom) {
					syslog(LOG_INFO,
				"print_output: short answer (%d, %d), zone %s",
					       cp - buf, n, zp->z_origin);
					error++;
					break;
				}
		
			} else {
#endif /*STUBS*/
				n = print_output(buf, bufsize, cp);
				if (cp + n != eom) {
					syslog(LOG_INFO,
				"print_output: short answer (%d, %d), zone %s",
					       cp - buf, n, zp->z_origin);
					error++;
					break;
				}
#ifdef STUBS
			}
#endif
			GETSHORT(n, tmp);
			if (n == T_SOA) {
				if (soacnt == 0) {
					soacnt++;
					if (dn_expand(buf, buf+PACKETSZ, nmp,
						      name, sizeof name) < 0) {
						badsoa_msg = "soa name error";
						goto badsoa;
					}
					if (strcasecmp(name, zp->z_origin)!=0){
						syslog(LOG_INFO,
			"wrong zone name in AXFR (wanted \"%s\", got \"%s\")",
						       zp->z_origin, name);
						badsoa_msg = "wrong soa name";
						goto badsoa;
					}
					if (eom - tmp
					    <= 2 * INT16SZ + INT32SZ) {
						badsoa_msg = "soa header";
						goto badsoa;
					}
					tmp += 2 * INT16SZ + INT32SZ;
					if ((n = dn_skipname(tmp, eom)) < 0) {
						badsoa_msg = "soa mname";
						goto badsoa;
					}
					tmp += n;
					if ((n = dn_skipname(tmp, eom)) < 0) {
						badsoa_msg = "soa hname";
						goto badsoa;
					}
					tmp += n;
					if (eom - tmp <= INT32SZ) {
						badsoa_msg = "soa dlen";
						goto badsoa;
					}
					GETLONG(serial, tmp);
					dprintf(3, (ddt,
					      "first SOA for %s, serial %lu\n",
						    name, (u_long)serial));
					continue;
				}
				if (dn_expand(buf, buf+PACKETSZ, nmp,
					      name2, sizeof name2) == -1) {
					badsoa_msg = "soa name error#2";
					goto badsoa;
				}
				if (strcasecmp((char *)name,
					       (char *)name2) != 0) {
					syslog(LOG_INFO,
				"got extra SOA for \"%s\" in zone \"%s\"",
					       name2, name);
					continue;
				}
				tmp -= INT16SZ;		/* Put TYPE back. */
				badsoa_msg = soa_zinfo(&zp_finish, tmp, eom);
				if (badsoa_msg)
					goto badsoa;
				dprintf(2, (ddt,
					    "SOA, serial %lu\n",
					    (u_long)zp_finish.z_serial));
				if (serial != zp_finish.z_serial) {
					soacnt = 0;
					got_soa = 0;
					minimum_ttl = 0;
					strcpy(prev_origin, zp->z_origin);
					prev_dname[0] = DEF_DNAME;
					dprintf(1, (ddt,
						    "serial changed, restart\n"
						    ));
					/*
					 * Flush buffer, truncate file
					 * and seek to beginning to restart.
					 */
					fflush(dbfp);
					if (ftruncate(fileno(dbfp), 0) != 0) {
						if (!quiet)
						    syslog(LOG_INFO,
							  "ftruncate %s: %m\n",
							   tmpname);
						return (XFER_FAIL);
					}
					fseek(dbfp, 0L, 0);
				} else
					break;
#ifdef STUBS
			} else if (zp->z_type == Z_STUB && n == T_NS) {
				nscnt++;
			} else if (zp->z_type == Z_STUB) {
				break;
#endif
			}
		}
		(void) my_close(s);
		if (error == 0) {
#ifdef POSIX_SIGNALS
			(void) sigaction(SIGALRM, &osv,
					 (struct sigaction *)0);
#else
			(void) sigvec(SIGALRM, &osv, (struct sigvec *)0);
#endif
			return (XFER_SUCCESS);
		}
		dprintf(2, (ddt, "error receiving zone transfer\n"));
	    } else if (zp_start.z_serial == serial_no) {
		(void) my_close(s);
		dprintf(1, (ddt,
			    "zone up-to-date, serial %u\n",
			    zp_start.z_serial));
		return (XFER_UPTODATE);
	    } else {
		(void) my_close(s);
		if (!quiet)
		    syslog(LOG_NOTICE,
		      "serial from [%s], zone %s: %u lower than current: %u\n",
			   inet_ntoa(sin.sin_addr), zp->z_origin,
			   zp_start.z_serial, serial_no);
		return (XFER_FAIL);
	    }
	}
#ifdef POSIX_SIGNALS
	(void) sigaction(SIGALRM, &osv, (struct sigaction *)0);
#else
	(void) sigvec(SIGALRM, &osv, (struct sigvec *)0);
#endif
	if (error)
		return (XFER_TIMEOUT);
	return (XFER_FAIL);
}

/*
 * Set flag saying to read was interrupted
 * used for a read timer
 */
static SIG_FN
read_alarm()
{
	read_interrupted = 1;
}
 
static int
netread(fd, buf, len, timeout)
	int fd;
	register char *buf;
	register int len;
	int timeout;
{
	static const char setitimerStr[] = "setitimer: %m";
	struct itimerval ival, zeroival;
	register int n;
#if defined(NETREAD_BROKEN)
	int retries = 0;
#endif

	memset(&zeroival, 0, sizeof zeroival);
	ival = zeroival;
	ival.it_value.tv_sec = timeout;
	while (len > 0) {
		if (setitimer(ITIMER_REAL, &ival, NULL) < 0) {
			syslog(LOG_INFO, setitimerStr);
			return (-1);
		}
		errno = 0;
		n = recv(fd, buf, len, 0);
		if (n == 0 && errno == 0) {
#if defined(NETREAD_BROKEN)
			if (++retries < 42)	/* doug adams */
				continue;
#endif
			syslog(LOG_INFO, "premature EOF, fetching \"%s\"",
			       domain);
			return (-1);
		}
		if (n < 0) {
			if (errno == 0) {
#if defined(NETREAD_BROKEN)
				if (++retries < 42)	/* doug adams */
					continue;
#endif
				syslog(LOG_INFO,
				       "recv(len=%d): n=%d && !errno",
				       len, n);
				return (-1);
			}
			if (errno == EINTR) {
				if (!read_interrupted) {
					/* It wasn't a timeout; ignore it. */
					continue;
				}
				errno = ETIMEDOUT;
			}
			syslog(LOG_INFO, "recv(len=%d): %m", len);
			return (-1);
		}
		buf += n;
		len -= n;
	}
	if (setitimer(ITIMER_REAL, &zeroival, NULL) < 0) {
		syslog(LOG_INFO, setitimerStr);
		return (-1);
	}
	return (0);
}

static const char *
soa_zinfo(zp, cp, eom)
	register struct zoneinfo *zp;
	register u_char *cp;
	u_char *eom;
{
	register int n;
	int type, class;
	u_long ttl;

	/* Are type, class, and ttl OK? */
	if (eom - cp < 3 * INT16SZ + INT32SZ)
		return ("zinfo too short");
	GETSHORT(type, cp);
	GETSHORT(class, cp);
	GETLONG(ttl, cp);
	cp += INT16SZ;	/* dlen */
	if (type != T_SOA || class != curclass)
		return ("zinfo wrong typ/cla/ttl");
	/* Skip master name and contact name, we can't validate them. */
	if ((n = dn_skipname(cp, eom)) == -1)
		return ("zinfo mname");
	cp += n;
	if ((n = dn_skipname(cp, eom)) == -1)
		return ("zinfo hname");
	cp += n;
	/* Grab the data fields. */
	if (eom - cp < 5 * INT32SZ)
		return ("zinfo dlen");
	GETLONG(zp->z_serial, cp);
	GETLONG(zp->z_refresh, cp);
	GETLONG(zp->z_retry, cp);
	GETLONG(zp->z_expire, cp);
	GETLONG(zp->z_minimum, cp);
	return (NULL);
}

/*
 * Parse the message, determine if it should be printed, and if so, print it
 * in .db file form.
 * Does minimal error checking on the message content.
 */
static int
print_output(msg, msglen, rrp)
	u_char *msg;
	int msglen;
	u_char *rrp;
{
	register u_char *cp;
	register HEADER *hp = (HEADER *) msg;
	u_int32_t addr, ttl;
	int i, j, tab, result, class, type, dlen, n1, n;
	char data[BUFSIZ];
	u_char *cp1, *cp2, *temp_ptr;
	char *cdata, *origin, *proto, dname[MAXDNAME];
	char *ignore = "";

	cp = rrp;
	n = dn_expand(msg, msg + msglen, cp, dname, sizeof dname);
	if (n < 0) {
		hp->rcode = FORMERR;
		return (-1);
	}
	cp += n;
	GETSHORT(type, cp);
	GETSHORT(class, cp);
	GETLONG(ttl, cp);
	GETSHORT(dlen, cp);

	origin = strchr(dname, '.');
	if (origin == NULL)
		origin = "";
	else
		origin++;	/* move past the '.' */
	dprintf(3, (ddt,
		    "print_output: dname %s type %d class %d ttl %d\n",
		    dname, type, class, ttl));
	/*
	 * Convert the resource record data into the internal database format.
	 */
	switch (type) {
	case T_A:
	case T_WKS:
	case T_HINFO:
	case T_UINFO:
	case T_TXT:
	case T_X25:
	case T_ISDN:
	case T_LOC:
	case T_NSAP:
	case T_UID:
	case T_GID:
		cp1 = cp;
		n = dlen;
		cp += n;
		break;

	case T_CNAME:
	case T_MB:
	case T_MG:
	case T_MR:
	case T_NS:
	case T_PTR:
		n = dn_expand(msg, msg + msglen, cp, data, sizeof data);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 = (u_char *)data;
		n = strlen(data) + 1;
		break;

	case T_MINFO:
	case T_SOA:
	case T_RP:
		n = dn_expand(msg, msg + msglen, cp, data, sizeof data);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		n = strlen(data) + 1;
		cp1 = (u_char *)data + n;
		n1 = sizeof data - n;
		if (type == T_SOA)
			n1 -= 5 * INT32SZ;
		n = dn_expand(msg, msg + msglen, cp, (char *)cp1, n1);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 += strlen((char *) cp1) + 1;
		if (type == T_SOA) {
			temp_ptr = cp + 4 * INT32SZ;
			GETLONG(minimum_ttl, temp_ptr);
			n = 5 * INT32SZ;
			bcopy((char *) cp, (char *) cp1, n);
			cp += n;
			cp1 += n;
		}
		n = cp1 - (u_char *)data;
		cp1 = (u_char *)data;
		break;

	case T_MX:
	case T_AFSDB:
	case T_RT:
		/* grab preference */
		bcopy((char *)cp, data, INT16SZ);
		cp1 = (u_char *)data + INT16SZ;
		cp += INT16SZ;

		/* get name */
		n = dn_expand(msg, msg + msglen, cp,
			      (char *)cp1, sizeof data - INT16SZ);
		if (n < 0)
			return (-1);
		cp += n;

		/* compute end of data */
		cp1 += strlen((char *) cp1) + 1;
		/* compute size of data */
		n = cp1 - (u_char *)data;
		cp1 = (u_char *)data;
		break;

	case T_PX:
		/* grab preference */
		bcopy((char *)cp, data, INT16SZ);
		cp1 = (u_char *)data + INT16SZ;
		cp += INT16SZ;

		/* get MAP822 name */
		n = dn_expand(msg, msg + msglen, cp,
			      (char *)cp1, sizeof data - INT16SZ);
		if (n < 0)
			return (-1);
		cp += n;
		cp1 += (n = (strlen((char *) cp1) + 1));
		n1 = sizeof data - n;

		/* get MAPX400 name */
		n = dn_expand(msg, msg + msglen, cp, (char *)cp1, n1);
		if (n < 0)
			return (-1);

		cp1 += strlen((char *) cp1) + 1;
		n = cp1 - (u_char *)data;
		cp1 = (u_char *)data;
		break;

	default:
		syslog(LOG_INFO, "\"%s %s %s\" - unknown type (%d)",
		       dname, p_class(class), p_type(type), type);
		hp->rcode = NOTIMP;
		return (-1);
	}
	if (n > MAXDATA) {
		dprintf(1, (ddt,
			    "update type %d: %d bytes is too much data\n",
			    type, n));
		hp->rcode = FORMERR;
		return (-1);
	}
	cdata = (char *) cp1;
	result = cp - rrp;

	/*
	 * Only print one SOA per db file
	 */
	if (type == T_SOA) {
		if (got_soa)
			return (result);
		else
			got_soa++;
	}

#ifdef NO_GLUE
	/*
	 * If they are trying to tell us info about something that is
	 * not in the zone that we are transfering, then ignore it!
	 * They don't have the authority to tell us this info.
	 *
	 * We have to do a bit of checking here - the name that we are
	 * checking vs is fully qualified & may be in a subdomain of the
	 * zone in question.  We also need to ignore any final dots.
	 *
	 * If a domain has both NS records and non-NS records, (for
	 * example, NS and MX records), then we should ignore the non-NS
	 * records (except that we should not ignore glue A records).
	 * XXX: It is difficult to do this properly, so we just compare
	 * the current dname with that in the most recent NS record.
	 * This defends against the most common error case,
	 * where the remote server sends MX records soon after the
	 * NS records for a particular domain.  If sent earlier, we lose. XXX
	 */
 	if (!samedomain(dname, domain)) {
		(void) fprintf(dbfp, "; Ignoring info about %s, not in zone %s.\n",
			dname, domain);
		ignore = "; ";
	} else if (type != T_NS && type != T_A &&
		   strcasecmp(zone_top, dname) != 0 &&
		   strcasecmp(prev_ns_dname, dname) == 0)
	{
		(void) fprintf(dbfp, "; Ignoring extra info about %s, invalid after NS delegation.\n",
			dname);
		ignore = "; ";
	}
#endif /*NO_GLUE*/

	/*
	 * If the current record is not being ignored, but the
	 * previous record was ignored, then we invalidate information
	 * that might have been altered by ignored records.
	 * (This means that we sometimes output unnecessary $ORIGIN
	 * lines, but that is harmless.)
	 * 
	 * Also update prev_comment now.
	 */
	if (prev_comment && ignore[0] == '\0') {
		prev_dname[0] = DEF_DNAME;
		prev_origin[0] = DEF_DNAME;
	}
	prev_comment = (ignore[0] != '\0');

	/*
	 * set prev_ns_dname if necessary
	 */
	if (type == T_NS) {
		(void) strcpy(prev_ns_dname, dname);
	}

	/*
	 * If the origin has changed, print the new origin
	 */
	if (strcasecmp(prev_origin, origin)) {
		(void) strcpy(prev_origin, origin);
		(void) fprintf(dbfp, "%s$ORIGIN %s.\n", ignore, origin);
	}
	tab = 0;

	if (strcasecmp(prev_dname, dname)) {
		/*
		 * set the prev_dname to be the current dname, then cut off all
		 * characters of dname after (and including) the first '.'
		 */
		char *cutp = strchr(dname, '.');

		(void) strcpy(prev_dname, dname);
		if (cutp)
			*cutp = '\0';

		if (dname[0] == 0) {
			if (origin[0] == 0)
				(void) fprintf(dbfp, "%s.\t", ignore);
			else
				(void) fprintf(dbfp, "%s.%s.\t",
					ignore, origin);	/* ??? */
		} else
			(void) fprintf(dbfp, "%s%s\t", ignore, dname);
		if (strlen(dname) < (size_t)8)
			tab = 1;
	} else {
		(void) fprintf(dbfp, "%s\t", ignore);
		tab = 1;
	}

	if (ttl != minimum_ttl)
		(void) fprintf(dbfp, "%d\t", (int) ttl);
	else if (tab)
		(void) putc('\t', dbfp);

	(void) fprintf(dbfp, "%s\t%s\t", p_class(class), p_type(type));
	cp = (u_char *) cdata;

	/*
	 * Print type specific data
	 */
	switch (type) {

	case T_A:
		switch (class) {
		case C_IN:
		case C_HS:
			GETLONG(n, cp);
			n = htonl(n);
			fputs(inet_ntoa(*(struct in_addr *) &n), dbfp);
			break;
		}
		(void) fprintf(dbfp, "\n");
		break;

	case T_CNAME:
	case T_MB:
	case T_MG:
	case T_MR:
	case T_PTR:
		if (cp[0] == '\0')
			(void) fprintf(dbfp, ".\n");
		else
			(void) fprintf(dbfp, "%s.\n", cp);
		break;

	case T_NS:
		cp = (u_char *) cdata;
		if (cp[0] == '\0')
			(void) fprintf(dbfp, ".\t");
		else
			(void) fprintf(dbfp, "%s.", cp);
		(void) fprintf(dbfp, "\n");
		break;

	case T_HINFO:
	case T_ISDN:
		cp2 = cp + n;
		for (i = 0; i < 2; i++) {
			if (i != 0)
				(void) putc(' ', dbfp);
			n = *cp++;
			cp1 = cp + n;
			if (cp1 > cp2)
				cp1 = cp2;
			(void) putc('"', dbfp);
			j = 0;
			while (cp < cp1) {
				if (*cp == '\0') {
					cp = cp1;
					break;
				}
				if ((*cp == '\n') || (*cp == '"')) {
					(void) putc('\\', dbfp);
				}
				(void) putc(*cp++, dbfp);
				j++;
			}
			if (j == 0 && (type != T_ISDN || i == 0))
				(void) putc('?', dbfp);
			(void) putc('"', dbfp);
		}
		(void) putc('\n', dbfp);
		break;

	case T_SOA:
		(void) fprintf(dbfp, "%s.", cp);
		cp += strlen((char *) cp) + 1;
		(void) fprintf(dbfp, " %s. (\n", cp);
		cp += strlen((char *) cp) + 1;
		GETLONG(n, cp);
		(void) fprintf(dbfp, "%s\t\t%lu", ignore, (u_long)n);
		GETLONG(n, cp);
		(void) fprintf(dbfp, " %lu", (u_long)n);
		GETLONG(n, cp);
		(void) fprintf(dbfp, " %lu", (u_long)n);
		GETLONG(n, cp);
		(void) fprintf(dbfp, " %lu", (u_long)n);
		GETLONG(n, cp);
		(void) fprintf(dbfp, " %lu )\n", (u_long)n);
		break;

	case T_MX:
	case T_AFSDB:
	case T_RT:
		GETSHORT(n, cp);
		(void) fprintf(dbfp, "%lu", (u_long)n);
		(void) fprintf(dbfp, " %s.\n", cp);
		break;

	case T_PX:
		GETSHORT(n, cp);
		(void) fprintf(dbfp, "%lu", (u_long)n);
		(void) fprintf(dbfp, " %s.", cp);
		cp += strlen((char *) cp) + 1;
		(void) fprintf(dbfp, " %s.\n", cp);
		break;

	case T_TXT:
	case T_X25:
		cp1 = cp + n;
		(void) putc('"', dbfp);
		while (cp < cp1) {
			if (i = *cp++) {
				for (j = i ; j > 0 && cp < cp1 ; j--) {
					if ((*cp == '\n') || (*cp == '"')) {
						(void) putc('\\', dbfp);
					}
					(void) putc(*cp++, dbfp);
				}
			}
		}
		(void) fputs("\"\n", dbfp);
		break;

	case T_NSAP:
		fprintf(dbfp, "%s\n", inet_nsap_ntoa(n, cp, NULL));
		break;

	case T_UINFO:
		(void) fprintf(dbfp, "\"%s\"\n", cp);
		break;

#ifdef LOC_RR
	case T_LOC:
		(void) fprintf(dbfp, "%s\n", loc_ntoa(cp, NULL));
		break;
#endif /* LOC_RR */

	case T_UID:
	case T_GID:
		if (n == INT32SZ) {
			GETLONG(n, cp);
			(void) fprintf(dbfp, "%lu\n", (u_long)n);
		}
		break;

	case T_WKS:
		GETLONG(addr, cp);
		addr = htonl(addr);
		fputs(inet_ntoa(*(struct in_addr *) &addr), dbfp);
		fputc(' ', dbfp);
		proto = protocolname(*cp);
		cp += sizeof(char);
		(void) fprintf(dbfp, "%s ", proto);
		i = 0;
		while (cp < (u_char *) cdata + n) {
			j = *cp++;
			do {
				if (j & 0200)
					(void) fprintf(dbfp, " %s",
						       servicename(i, proto));
				j <<= 1;
			} while (++i & 07);
		}
		(void) fprintf(dbfp, "\n");
		break;

	case T_MINFO:
	case T_RP:
		(void) fprintf(dbfp, "%s.", cp);
		cp += strlen((char *) cp) + 1;
		(void) fprintf(dbfp, " %s.\n", cp);
		break;

	default:
		(void) fprintf(dbfp, "???\n");
	}
	if (ferror(dbfp)) {
		syslog(LOG_ERR, "%s: %m", tmpname);
		exit(XFER_FAIL);
	}
	return (result);
}

#ifdef SHORT_FNAMES
/*
** This routine handles creating temporary files with mkstemp
** in the presence of a 14 char filename system.  Pathconf()
** does not work over NFS.
*/
filenamecpy(ddtfile, optarg)
char *ddtfile, *optarg;
{
	int namelen, extra, len;
	char *dirname, *filename;

	/* determine the length of filename allowed */
	if((dirname = strrchr(optarg, '/')) == NULL){
		filename = optarg;
	} else {
		*dirname++ = '\0';
		filename = dirname;
	}
	namelen = pathconf(dirname == NULL? "." : optarg, _PC_NAME_MAX);
	if(namelen <= 0)
		namelen = 255;  /* length could not be determined */
	if(dirname != NULL)
		*--dirname = '/';

	/* copy a shorter name if it will be longer than allowed */
	extra = (strlen(filename)+strlen(".XXXXXX")) - namelen;
	if(extra > 0){
		len = strlen(optarg) - extra;
		(void) strncpy(ddtfile, optarg, len);
		ddtfile[len] = '\0';
	} else
		(void) strcpy(ddtfile, optarg);
}
#endif /* SHORT_FNAMES */
