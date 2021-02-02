/*	$OpenBSD: ftp_var.h,v 1.45 2020/09/01 12:33:48 jca Exp $	*/
/*	$NetBSD: ftp_var.h,v 1.18 1997/08/18 10:20:25 lukem Exp $	*/

/*
 * Copyright (C) 1997 and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1985, 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)ftp_var.h	8.4 (Berkeley) 10/9/94
 */

/*
 * FTP global variables.
 */

#include <sys/signal.h>
#include <limits.h>
#include <setjmp.h>

#ifndef SMALL
#include <histedit.h>
#endif /* !SMALL */

#include <tls.h>

#include "stringlist.h"
#include "extern.h"
#include "small.h"

#define HASHBYTES	1024
#define FTPBUFLEN	PATH_MAX + 200

#define STALLTIME	5	/* # of seconds of no xfer before "stalling" */

#define	FTP_PORT	21	/* default if ! getservbyname("ftp/tcp") */
#define	GATE_PORT	21	/* default if ! getservbyname("ftpgate/tcp") */
#define	HTTP_PORT	80	/* default if ! getservbyname("http/tcp") */
#define	HTTPS_PORT	443	/* default if ! getservbyname("https/tcp") */
#define	HTTP_USER_AGENT	"User-Agent: OpenBSD ftp"	/* User-Agent string sent to web server */

#define PAGER		"more"	/* default pager if $PAGER isn't set */

/*
 * Options and other state info.
 */
extern int	trace;		/* trace packets exchanged */
extern int	hash;		/* print # for each buffer transferred */
extern int	mark;		/* number of bytes between hashes */
extern int	sendport;	/* use PORT/LPRT cmd for each data connection */
extern int	verbose;	/* print messages coming back from server */
extern int	connected;	/* 1 = connected to server, -1 = logged in */
extern int	fromatty;	/* input is from a terminal */
extern int	interactive;	/* interactively prompt on m* cmds */
#ifndef SMALL
extern int	confirmrest;	/* confirm rest of current m* cmd */
extern int	debug;		/* debugging level */
extern int	bell;		/* ring bell on cmd completion */
extern char   *altarg;		/* argv[1] with no shell-like preprocessing  */
#endif /* !SMALL */
extern int	doglob;		/* glob local file names */
extern int	autologin;	/* establish user account on connection */
extern int	proxy;		/* proxy server connection active */
extern int	proxflag;	/* proxy connection exists */
extern int	gatemode;	/* use gate-ftp */
extern char   *gateserver;	/* server to use for gate-ftp */
extern int	sunique;	/* store files on server with unique name */
extern int	runique;	/* store local files with unique name */
extern int	mcase;		/* map upper to lower case for mget names */
extern int	ntflag;		/* use ntin ntout tables for name translation */
extern int	mapflag;	/* use mapin mapout templates on file names */
extern int	preserve;	/* preserve modification time on files */
extern int	progress;	/* display transfer progress bar */
extern int	code;		/* return/reply code for ftp command */
extern int	crflag;		/* if 1, strip car. rets. on ascii gets */
extern char	pasv[BUFSIZ];	/* passive port for proxy data connection */
extern int	passivemode;	/* passive mode enabled */
extern int	activefallback;	/* fall back to active mode if passive fails */
extern char	ntin[17];	/* input translation table */
extern char	ntout[17];	/* output translation table */
extern char	mapin[PATH_MAX];	/* input map template */
extern char	mapout[PATH_MAX];	/* output map template */
extern char	typename[32];	/* name of file transfer type */
extern int	type;		/* requested file transfer type */
extern int	curtype;	/* current file transfer type */
extern char	structname[32];	/* name of file transfer structure */
extern int	stru;		/* file transfer structure */
extern char	formname[32];	/* name of file transfer format */
extern int	form;		/* file transfer format */
extern char	modename[32];	/* name of file transfer mode */
extern int	mode;		/* file transfer mode */
extern char	bytename[32];	/* local byte size in ascii */
extern int	bytesize;	/* local byte size in binary */
extern int	anonftp;	/* automatic anonymous login */
extern int	dirchange;	/* remote directory changed by cd command */
extern unsigned int retry_connect;	/* retry connect if failed */
extern int	ttywidth;	/* width of tty */
extern int	epsv4;		/* use EPSV/EPRT on IPv4 connections */
extern int	epsv4bad;	/* EPSV doesn't work on the current server */

#ifndef SMALL
extern int	  editing;	/* command line editing enabled */
extern EditLine *el;		/* editline(3) status structure */
extern History  *hist;		/* editline(3) history structure */
extern char	 *cursor_pos;	/* cursor position we're looking for */
extern size_t	  cursor_argc;	/* location of cursor in margv */
extern size_t	  cursor_argo;	/* offset of cursor in margv[cursor_argc] */
extern int	  resume;	/* continue transfer */
extern int	  timestamp;    /* send an If-Modified-Since header */
extern char	 *srcaddr;	/* source address to bind to */
#endif /* !SMALL */

extern char	 *cookiefile;	/* cookie jar to use */

extern off_t	bytes;		/* current # of bytes read */
extern off_t	filesize;	/* size of file being transferred */
extern char   *direction;	/* direction transfer is occurring */

extern char   *hostname;	/* name of host connected to */
extern int	unix_server;	/* server is unix, can use binary for ascii */
extern int	unix_proxy;	/* proxy is unix, can use binary for ascii */

extern char *ftpport;		/* port number to use for ftp connections */
extern char *httpport;		/* port number to use for http connections */
#ifndef NOSSL
extern char *httpsport;		/* port number to use for https connections */
#endif /* !SMALL */
extern char *httpuseragent;	/* user agent for http(s) connections */
extern char *gateport;		/* port number to use for gateftp connections */

extern jmp_buf	toplevel;	/* non-local goto stuff for cmd scanner */

#ifndef SMALL
extern char	line[FTPBUFLEN];	/* input line buffer */
extern char	*argbase;	/* current storage point in arg buffer */
extern char	*stringbase;	/* current scan point in line buffer */
extern char	argbuf[FTPBUFLEN];	/* argument storage buffer */
extern StringList *marg_sl;	/* stringlist containing margv */
extern int	margc;		/* count of arguments on input line */
extern int	options;	/* used during socket creation */
#endif /* !SMALL */

#define margv (marg_sl->sl_str)	/* args parsed from input line */
extern int	cpend;		/* flag: if != 0, then pending server reply */
extern int	mflag;		/* flag: if != 0, then active multi command */

/*
 * Format of command table.
 */
struct cmd {
	char	*c_name;	/* name of command */
	char	*c_help;	/* help string */
	char	 c_bell;	/* give bell when command completes */
	char	 c_conn;	/* must be connected to use command */
	char	 c_proxy;	/* proxy server may execute */
#ifndef SMALL
	char	*c_complete;	/* context sensitive completion list */
#endif /* !SMALL */
	void	(*c_handler)(int, char **); /* function to call */
};

struct macel {
	char mac_name[9];	/* macro name */
	char *mac_start;	/* start of macro in macbuf */
	char *mac_end;		/* end of macro in macbuf */
};

#ifndef SMALL
extern int macnum;		/* number of defined macros */
extern struct macel macros[16];
extern char macbuf[4096];
#endif /* !SMALL */

extern FILE	*ttyout;	/* stdout or stderr, depending on interactive */

extern struct cmd cmdtab[];

#ifndef NOSSL
extern struct tls_config *tls_config;
extern int tls_session_fd;
#endif /* !NOSSL */
