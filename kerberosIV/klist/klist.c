/*	$Id$	*/

/*-
 * Copyright 1987, 1988 by the Student Information Processing Board
 *	of the Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is
 * hereby granted, provided that the above copyright notice
 * appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation,
 * and that the names of M.I.T. and the M.I.T. S.I.P.B. not be
 * used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 * M.I.T. and the M.I.T. S.I.P.B. make no representations about
 * the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 */

/*
 * Lists your current Kerberos tickets.
 * Written by Bill Sommerfeld, MIT Project Athena.
 */

#include <kuser_locl.h>

char   *whoami;			/* What was I invoked as?? */

static char *
short_date(dp)
    time_t *dp;
{
    register char *cp;

    if (*dp == (time_t)(-1L)) return "***  Never  *** ";
    cp = ctime(dp) + 4;
    cp[15] = '\0';
    return (cp);
}

static void
display_tktfile(file, tgt_test, long_form)
char *file;
int tgt_test, long_form;
{
    char    pname[ANAME_SZ];
    char    pinst[INST_SZ];
    char    prealm[REALM_SZ];
    char    buf1[20], buf2[20];
    int     k_errno;
    CREDENTIALS c;
    int     header = 1;

    if ((file == NULL) && ((file = getenv("KRBTKFILE")) == NULL))
	file = TKT_FILE;

    if (long_form)
	printf("Ticket file:	%s\n", file);

    /* 
     * Since krb_get_tf_realm will return a ticket_file error, 
     * we will call tf_init and tf_close first to filter out
     * things like no ticket file.  Otherwise, the error that 
     * the user would see would be 
     * klist: can't find realm of ticket file: No ticket file (tf_util)
     * instead of
     * klist: No ticket file (tf_util)
     */

    /* Open ticket file */
    if ((k_errno = tf_init(file, R_TKT_FIL))) {
	if (!tgt_test)
		fprintf(stderr, "%s: %s\n", whoami, krb_err_txt[k_errno]);
	exit(1);
    }
    /* Close ticket file */
    (void) tf_close();

    /* 
     * We must find the realm of the ticket file here before calling
     * tf_init because since the realm of the ticket file is not
     * really stored in the principal section of the file, the
     * routine we use must itself call tf_init and tf_close.
     */
    if ((k_errno = krb_get_tf_realm(file, prealm)) != KSUCCESS) {
	if (!tgt_test)
	    fprintf(stderr, "%s: can't find realm of ticket file: %s\n",
		    whoami, krb_err_txt[k_errno]);
	exit(1);
    }

    /* Open ticket file */
    if ((k_errno = tf_init(file, R_TKT_FIL))) {
	if (!tgt_test)
		fprintf(stderr, "%s: %s\n", whoami, krb_err_txt[k_errno]);
	exit(1);
    }
    /* Get principal name and instance */
    if ((k_errno = tf_get_pname(pname)) ||
	(k_errno = tf_get_pinst(pinst))) {
	    if (!tgt_test)
		    fprintf(stderr, "%s: %s\n", whoami, krb_err_txt[k_errno]);
	    exit(1);
    }

    /* 
     * You may think that this is the obvious place to get the
     * realm of the ticket file, but it can't be done here as the
     * routine to do this must open the ticket file.  This is why 
     * it was done before tf_init.
     */
       
    if (!tgt_test && long_form)
	printf("Principal:\t%s%s%s%s%s\n\n", pname,
	       (pinst[0] ? "." : ""), pinst,
	       (prealm[0] ? "@" : ""), prealm);
    while ((k_errno = tf_get_cred(&c)) == KSUCCESS) {
	if (!tgt_test && long_form && header) {
	    printf("%-15s  %-15s  %s\n",
		   "  Issued", "  Expires", "  Principal");
	    header = 0;
	}
	if (tgt_test) {
	    c.issue_date = krb_life_to_time(c.issue_date, c.lifetime);
	    if (!strcmp(c.service, TICKET_GRANTING_TICKET) &&
		!strcmp(c.instance, prealm)) {
		if (time(0) < c.issue_date)
		    exit(0);		/* tgt hasn't expired */
		else
		    exit(1);		/* has expired */
	    }
	    continue;			/* not a tgt */
	}
	if (long_form) {
	    (void) strcpy(buf1, short_date(&c.issue_date));
	    c.issue_date = krb_life_to_time(c.issue_date, c.lifetime);
	    if (time(0) < (unsigned long) c.issue_date)
	        (void) strcpy(buf2, short_date(&c.issue_date));
	    else
	        (void) strcpy(buf2, ">>> Expired <<< ");
	    printf("%s  %s  ", buf1, buf2);
	}
	printf("%s%s%s%s%s\n",
	       c.service, (c.instance[0] ? "." : ""), c.instance,
	       (c.realm[0] ? "@" : ""), c.realm);
    }
    if (tgt_test)
	exit(1);			/* no tgt found */
    if (header && long_form && k_errno == EOF) {
	printf("No tickets in file.\n");
    }
}

/* adapted from getst() in librkb */
/*
 * ok_getst() takes a file descriptor, a string and a count.  It reads
 * from the file until either it has read "count" characters, or until
 * it reads a null byte.  When finished, what has been read exists in
 * the given string "s".  If "count" characters were actually read, the
 * last is changed to a null, so the returned string is always null-
 * terminated.  ok_getst() returns the number of characters read, including
 * the null terminator.
 *
 * If there is a read error, it returns -1 (like the read(2) system call)
 */

static int
ok_getst(fd, s, n)
    int fd;
    register char *s;
    int n;
{
    register count = n;
    int err;
    while ((err = read(fd, s, 1)) > 0 && --count)
        if (*s++ == '\0')
            return (n - count);
    if (err < 0)
	return(-1);
    *s = '\0';
    return (n - count);
}

static void
display_srvtab(file)
char *file;
{
    int stab;
    char serv[SNAME_SZ];
    char inst[INST_SZ];
    char rlm[REALM_SZ];
    unsigned char key[8];
    unsigned char vno;
    int count;

    printf("Server key file:   %s\n", file);
	
    if ((stab = open(file, O_RDONLY, 0400)) < 0) {
	perror(file);
	exit(1);
    }
    printf("%-15s %-15s %-10s %s\n","Service","Instance","Realm",
	   "Key Version");
    printf("------------------------------------------------------\n");

    /* argh. getst doesn't return error codes, it silently fails */
    while (((count = ok_getst(stab, serv, SNAME_SZ)) > 0)
	   && ((count = ok_getst(stab, inst, INST_SZ)) > 0)
	   && ((count = ok_getst(stab, rlm, REALM_SZ)) > 0)) {
	if (((count = read(stab,(char *) &vno,1)) != 1) ||
	     ((count = read(stab,(char *) key,8)) != 8)) {
	    if (count < 0)
		perror("reading from key file");
	    else
		fprintf(stderr, "key file truncated\n");
	    exit(1);
	}
	printf("%-15s %-15s %-15s %d\n",serv,inst,rlm,vno);
    }
    if (count < 0)
	perror(file);
    (void) close(stab);
}

static void
usage()
{
    fprintf(stderr,
        "Usage: %s [ -s | -t ] [ -file filename ] [ -srvtab ]\n", whoami);
    exit(1);
}

/* ARGSUSED */
int
main(argc, argv)
    int     argc;
    char  **argv;
{
    int     long_form = 1;
    int     tgt_test = 0;
    int     do_srvtab = 0;
    char   *tkt_file = NULL;
    char   *cp;

    whoami = (cp = strrchr(*argv, '/')) ? cp + 1 : *argv;

    while (*(++argv)) {
	if (!strcmp(*argv, "-s")) {
	    long_form = 0;
	    continue;
	}
	if (!strcmp(*argv, "-t")) {
	    tgt_test = 1;
	    long_form = 0;
	    continue;
	}
	if (!strcmp(*argv, "-l")) {	/* now default */
	    continue;
	}
	if (!strcmp(*argv, "-file")) {
	    if (*(++argv)) {
		tkt_file = *argv;
		continue;
	    } else
		usage();
	}
	if (!strcmp(*argv, "-srvtab")) {
		if (tkt_file == NULL)	/* if no other file spec'ed,
					   set file to default srvtab */
		    tkt_file = KEYFILE;
		do_srvtab = 1;
		continue;
	}
	usage();
    }

    if (do_srvtab)
	display_srvtab(tkt_file);
    else
	display_tktfile(tkt_file, tgt_test, long_form);
    exit(0);
}
