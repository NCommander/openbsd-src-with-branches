/*	$OpenBSD: passwd.c,v 1.10 2001/06/26 20:16:43 hin Exp $	*/

/*
 * Copyright (c) 1988 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1988 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static const char sccsid[] = "from: @(#)passwd.c	5.5 (Berkeley) 7/6/91";*/
static const char rcsid[] = "$OpenBSD: passwd.c,v 1.10 2001/06/26 20:16:43 hin Exp $";
#endif /* not lint */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifdef KERBEROS
#include <kerberosIV/krb.h>
#endif

/*
 * Note on configuration:
 *      Generally one would not use both Kerberos and YP
 *      to maintain passwords.
 *
 */

int use_kerberos;
int use_yp;

#ifdef YP
int force_yp;
#endif


extern int local_passwd(char *, int);
extern int yp_passwd(char *);
extern int krb_passwd(int, char **);
extern int krb5_passwd(int, char **);
void usage(int value);


int
main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	register int ch;
	char *username;
	int status = 0;
#if defined(KERBEROS) || defined(KERBEROS5)
	extern char realm[];

	if (krb_get_lrealm(realm,1) == KSUCCESS)
		use_kerberos = 1;
#endif
#ifdef	YP
	use_yp = _yp_check(NULL);
#endif

	/* Process args and options */
	while ((ch = getopt(argc, argv, "lykK")) != -1)
		switch (ch) {
		case 'l':		/* change local password file */
			use_kerberos = 0;
			use_yp = 0;
			break;
		case 'k':		/* change Kerberos password */
#if defined(KERBEROS)
			use_kerberos = 1;
			use_yp = 0;
			exit(krb_passwd(argc, argv));
			break;
#else
			fprintf(stderr, "passwd: Kerberos not compiled in\n");
			exit(1);
#endif
		case 'K':
#ifdef KRB5
			/* Skip programname and '-K' option */
			argc-=2;
			argv+=2;
			exit(krb5_passwd(argc, argv));
#else			
			errx(1, "KerberosV support not enabled");
			break;
#endif
		case 'y':		/* change YP password */
#ifdef	YP
			if (!use_yp) {
				fprintf(stderr, "passwd: YP not in use.\n");
				exit(1);
			}
			use_kerberos = 0;
			use_yp = 1;
			force_yp = 1;
			break;
#else
			fprintf(stderr, "passwd: YP not compiled in\n");
			exit(1);
#endif
		default:
			usage(1);
		}

	argc -= optind;
	argv += optind;

	username = getlogin();
	if (username == NULL) {
		fprintf(stderr, "passwd: who are you ??\n");
		exit(1);
	}
	
	switch(argc) {
	case 0:
		break;
	case 1:
#if defined(KERBEROS) || defined(KERBEROS5)
	    if (use_kerberos && strcmp(argv[0], username)) {
		(void)fprintf(stderr, "passwd: %s\n\t%s\n%s\n",
			      "to change another user's Kerberos password, do",
			      "\"passwd -k -u <user>\";",
			      "to change a user's local passwd, use \"passwd -l <user>\"");
		exit(1);
	    }
#endif
		username = argv[0];
		break;
	default:
		usage(1);
	}

#if defined(KERBEROS) || defined(KERBEROS5)
        if (use_kerberos)
                exit(krb_passwd(argc, argv));
#endif

#ifdef	YP
	if (force_yp || ((status = local_passwd(username, 0)) && use_yp))
		exit(yp_passwd(username));
	exit(status);
#endif
	exit(local_passwd(username, 0));
}

void
usage(retval)
	int retval;
{
	fprintf(stderr, "usage: passwd [-l] [-y] [-k [-n name] [-i instance] [-r realm] [-u username[.instance][@realm]] [user]\n");
	exit(retval);
}
