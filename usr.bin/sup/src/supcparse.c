/*
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software_Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 * sup collection parsing routines
 **********************************************************************
 * HISTORY
 *
 * 7-July-93  Nate Williams at Montana State University
 *	Modified SUP to use gzip based compression when sending files
 *	across the network to save BandWidth
 *
 * $Log: supcparse.c,v $
 * Revision 1.4  1995/06/09 04:33:34  christos
 * fixed bug related to the ascii `when' files previous fix, where the -t
 * option would fail to read the timestamp files.
 *
 * Revision 1.2  1993/08/04  17:46:20  brezak
 * Changes from nate for gzip'ed sup
 *
 * Revision 1.1.1.1  1993/05/21  14:52:18  cgd
 * initial import of CMU's SUP to NetBSD
 *
 * Revision 1.6  92/08/11  12:07:38  mrt
 * 	Added use-rel-suffix option corresponding to -u switch.
 * 	[92/07/26            mrt]
 * 
 * Revision 1.5  92/02/08  18:24:19  mja
 * 	Added "keep" supfile option, corresponding to -k switch.
 * 	[92/01/17            vdelvecc]
 * 
 * Revision 1.4  91/05/16  14:49:50  ern
 * 	Change default timeout from none to 3 hours so we don't accumalute 
 * 	processes running sups to dead hosts especially for users.
 * 	[91/05/16  14:49:21  ern]
 * 
 *
 * 10-Feb-88  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added timeout to backoff.
 *
 * 28-Jun-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added code for "release" support.  Removed obsolete options.
 *
 * 25-May-87  Doug Philips (dwp) at Carnegie-Mellon University
 *	Split off from sup.c
 *
 **********************************************************************
 */

#include "supcdefs.h"


#ifdef	lint
static char _argbreak;
#else
extern char _argbreak;			/* break character from nxtarg */
#endif

typedef enum {				/* supfile options */
	OHOST, OBASE, OHOSTBASE, OPREFIX, ORELEASE,
	ONOTIFY, OLOGIN, OPASSWORD, OCRYPT,
	OBACKUP, ODELETE, OEXECUTE, OOLD, OTIMEOUT, OKEEP, OURELSUF,
	OCOMPRESS
} OPTION;

struct option {
	char *op_name;
	OPTION op_enum;
} options[] = {
	"host",		OHOST,
	"base",		OBASE,
	"hostbase",	OHOSTBASE,
	"prefix",	OPREFIX,
	"release",	ORELEASE,
	"notify",	ONOTIFY,
	"login",	OLOGIN,
	"password",	OPASSWORD,
	"crypt",	OCRYPT,
	"backup",	OBACKUP,
	"delete",	ODELETE,
	"execute",	OEXECUTE,
	"old",		OOLD,
	"timeout",	OTIMEOUT,
	"keep",		OKEEP,
	"use-rel-suffix", OURELSUF,
 	"compress", 	OCOMPRESS
};

passdelim (ptr,delim)		/* skip over delimiter */
char **ptr,delim;
{
	*ptr = skipover (*ptr, " \t");
	if (_argbreak != delim && **ptr == delim) {
		(*ptr)++;
		*ptr = skipover (*ptr, " \t");
	}
}

parsecoll(c,collname,args)
COLLECTION *c;
char *collname,*args;
{
	register char *arg,*p;
	register OPTION option;
	int opno;

	c->Cnext = NULL;
	c->Cname = salloc (collname);
	c->Chost = NULL;
	c->Chtree = NULL;
	c->Cbase = NULL;
	c->Chbase = NULL;
	c->Cprefix = NULL;
	c->Crelease = NULL;
	c->Cnotify = NULL;
	c->Clogin = NULL;
	c->Cpswd = NULL;
	c->Ccrypt = NULL;
	c->Ctimeout = 3*60*60;	/* default to 3 hours instead of no timeout */
	c->Cflags = 0;
	c->Cnogood = FALSE;
	c->Clockfd = -1;
	args = skipover (args," \t");
	while (*(arg=nxtarg(&args," \t="))) {
		for (opno = 0; opno < sizeofA(options); opno++)
			if (strcmp (arg,options[opno].op_name) == 0)
				break;
		if (opno == sizeofA(options)) {
			logerr ("Invalid supfile option %s for collection %s",
				arg,c->Cname);
			return(-1);
		}
		option = options[opno].op_enum;
		switch (option) {
		case OHOST:
			passdelim (&args,'=');
			do {
				arg = nxtarg (&args,", \t");
				(void) Tinsert (&c->Chtree,arg,FALSE);
				arg = args;
				p = skipover (args," \t");
				if (*p++ == ',')  args = p;
			} while (arg != args);
			break;
		case OBASE:
			passdelim (&args,'=');
			arg = nxtarg (&args," \t");
			c->Cbase = salloc (arg);
			break;
		case OHOSTBASE:
			passdelim (&args,'=');
			arg = nxtarg (&args," \t");
			c->Chbase = salloc (arg);
			break;
		case OPREFIX:
			passdelim (&args,'=');
			arg = nxtarg (&args," \t");
			c->Cprefix = salloc (arg);
			break;
		case ORELEASE:
			passdelim (&args,'=');
			arg = nxtarg (&args," \t");
			c->Crelease = salloc (arg);
			break;
		case ONOTIFY:
			passdelim (&args,'=');
			arg = nxtarg (&args," \t");
			c->Cnotify = salloc (arg);
			break;
		case OLOGIN:
			passdelim (&args,'=');
			arg = nxtarg (&args," \t");
			c->Clogin = salloc (arg);
			break;
		case OPASSWORD:
			passdelim (&args,'=');
			arg = nxtarg (&args," \t");
			c->Cpswd = salloc (arg);
			break;
		case OCRYPT:
			passdelim (&args,'=');
			arg = nxtarg (&args," \t");
			c->Ccrypt = salloc (arg);
			break;
		case OBACKUP:
			c->Cflags |= CFBACKUP;
			break;
		case ODELETE:
			c->Cflags |= CFDELETE;
			break;
		case OEXECUTE:
			c->Cflags |= CFEXECUTE;
			break;
		case OOLD:
			c->Cflags |= CFOLD;
			break;
		case OKEEP:
			c->Cflags |= CFKEEP;
			break;
		case OURELSUF:
			c->Cflags |= CFURELSUF;
			break;
		case OCOMPRESS:
			c->Cflags |= CFCOMPRESS;
			break;
		case OTIMEOUT:
			passdelim (&args,'=');
			arg = nxtarg (&args," \t");
			c->Ctimeout = atoi (arg);
			break;
		}
	}
	return(0);
}


long
getwhen(collection, relsuffix)
	char *collection, *relsuffix;
{
	char buf[STRINGLENGTH];
	char *ep;
	FILE *fp;
	long tstamp;

	(void) sprintf (buf,FILEWHEN,collection,relsuffix);

	if ((fp = fopen(buf, "r")) == NULL)
		return 0;

	if (fgets(buf, sizeof(buf), fp) == NULL) {
		(void) fclose(fp);
		return 0;
	}

	(void) fclose(fp);

	if ((tstamp = strtol(buf, &ep, 0)) == -1 || *ep != '\n')
		return 0;

	return tstamp;
}

int
putwhen(fname, tstamp)
	char *fname;
	long tstamp;
{
	FILE *fp;
	if ((fp = fopen(fname, "w")) == NULL)
		return 0;
	if (fprintf(fp, "%ld\n", tstamp) < 0)
		return 0;
	if (fclose(fp) != 0)
		return 0;
	return 1;
}
