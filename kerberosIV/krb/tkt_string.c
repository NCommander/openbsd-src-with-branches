/*
 * This software may now be redistributed outside the US.
 *
 * $Source: /cvs/src/kerberosIV/krb/tkt_string.c,v $
 *
 * $Locker:  $
 */

/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */

#include "krb_locl.h"

#include <sys/param.h>
#include <sys/types.h>

/*
 * This routine is used to generate the name of the file that holds
 * the user's cache of server tickets and associated session keys.
 *
 * If it is set, krb_ticket_string contains the ticket file name.
 * Otherwise, the filename is constructed as follows:
 *
 * If it is set, the environment variable "KRBTKFILE" will be used as
 * the ticket file name.  Otherwise TKT_ROOT (defined in "krb.h") and
 * the user's uid are concatenated to produce the ticket file name
 * (e.g., "/tmp/tkt123").  A pointer to the string containing the ticket
 * file name is returned.
 */

static char krb_ticket_string[MAXPATHLEN] = "";

char *
tkt_string()
{
    char *env;
    uid_t getuid(void);

    if (!*krb_ticket_string) {
        if ((env = getenv("KRBTKFILE"))) {
	    (void) strncpy(krb_ticket_string, env,
			   sizeof(krb_ticket_string)-1);
	    krb_ticket_string[sizeof(krb_ticket_string)-1] = '\0';
	} else {
	    /* 32 bits of signed integer will always fit in 11 characters
	     (including the sign), so no need to worry about overflow */
	    (void) snprintf(krb_ticket_string, sizeof(krb_ticket_string),
	    		    "%s%u", TKT_ROOT, getuid());
        }
    }
    return krb_ticket_string;
}

/*
 * This routine is used to set the name of the file that holds the user's
 * cache of server tickets and associated session keys.
 *
 * The value passed in is copied into local storage.
 *
 * NOTE:  This routine should be called during initialization, before other
 * Kerberos routines are called; otherwise tkt_string() above may be called
 * and return an undesired ticket file name until this routine is called.
 */

void
krb_set_tkt_string(val)
	char *val;
{

    (void) strncpy(krb_ticket_string, val, sizeof(krb_ticket_string)-1);
    krb_ticket_string[sizeof(krb_ticket_string)-1] = '\0';

    return;
}
