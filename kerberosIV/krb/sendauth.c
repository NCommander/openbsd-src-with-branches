/*
 * This software may now be redistributed outside the US.
 *
 * $Source: /cvs/src/kerberosIV/krb/sendauth.c,v $
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

#include <sys/types.h>
#include <netinet/in.h>
#include <syslog.h>

/*
 * If the protocol changes, you will need to change the version string
 * and make appropriate changes in krb_recvauth.c
 */
#define	KRB_SENDAUTH_VERS "AUTHV0.1" /* MUST be KRB_SENDAUTH_VLEN chars */


/*
 * This file contains two routines: krb_sendauth() and krb_sendsrv().
 *
 * krb_sendauth() transmits a ticket over a file descriptor for a
 * desired service, instance, and realm, doing mutual authentication
 * with the server if desired.
 *
 * krb_sendsvc() sends a service name to a remote knetd server.
 */

/*
 * The first argument to krb_sendauth() contains a bitfield of
 * options (the options are defined in "krb.h"):
 *
 * KOPT_DONT_CANON	Don't canonicalize instance as a hostname.
 *			(If this option is not chosen, krb_get_phost()
 *			is called to canonicalize it.)
 *
 * KOPT_DONT_MK_REQ 	Don't request server ticket from Kerberos.
 *			A ticket must be supplied in the "ticket"
 *			argument.
 *			(If this option is not chosen, and there
 *			is no ticket for the given server in the
 *			ticket cache, one will be fetched using
 *			krb_mk_req() and returned in "ticket".)
 *
 * KOPT_DO_MUTUAL	Do mutual authentication, requiring that the
 * 			receiving server return the checksum+1 encrypted
 *			in the session key.  The mutual authentication
 *			is done using krb_mk_priv() on the other side
 *			(see "recvauth.c") and krb_rd_priv() on this
 *			side.
 *
 * The "fd" argument is a file descriptor to write to the remote
 * server on.  The "ticket" argument is used to store the new ticket
 * from the krb_mk_req() call. If the KOPT_DONT_MK_REQ options is
 * chosen, the ticket must be supplied in the "ticket" argument.
 * The "service", "inst", and "realm" arguments identify the ticket.
 * If "realm" is null, the local realm is used.
 *
 * The following arguments are only needed if the KOPT_DO_MUTUAL option
 * is chosen:
 *
 *   The "checksum" argument is a number that the server will add 1 to
 *   to authenticate itself back to the client; the "msg_data" argument
 *   holds the returned mutual-authentication message from the server
 *   (i.e., the checksum+1); the "cred" structure is used to hold the
 *   session key of the server, extracted from the ticket file, for use
 *   in decrypting the mutual authentication message from the server;
 *   and "schedule" holds the key schedule for that decryption.  The
 *   the local and server addresses are given in "laddr" and "faddr".
 *
 * The application protocol version number (of up to KRB_SENDAUTH_VLEN
 * characters) is passed in "version".
 *
 * If all goes well, KSUCCESS is returned, otherwise some error code.
 *
 * The format of the message sent to the server is:
 *
 * Size			Variable		Field
 * ----			--------		-----
 *
 * KRB_SENDAUTH_VLEN	KRB_SENDAUTH_VER	sendauth protocol
 * bytes					version number
 *
 * KRB_SENDAUTH_VLEN	version			application protocol
 * bytes					version number
 *
 * 4 bytes		ticket->length		length of ticket
 *
 * ticket->length	ticket->dat		ticket itself
 */

/*
 * XXX: Note that krb_rd_priv() is coded in such a way that
 * "msg_data->app_data" will be pointing into "priv_buf", which
 * will disappear when krb_sendauth() returns.
 */

int
krb_sendauth(options, fd, ticket, service, inst, realm, checksum,
	     msg_data, cred, schedule, laddr, faddr, version)
	int32_t options;	/* bit-pattern of options */
	int fd;			/* file descriptor to write onto */
	KTEXT ticket;		/* where to put ticket (return); or
				 * supplied in case of KOPT_DONT_MK_REQ */
	char *service;		/* service name, instance, realm */
	char *inst;		/* checksum to include in request */
	char *realm;		/* mutual auth MSG_DAT (return) */
	u_int32_t checksum;	/* credentials (return) */
	MSG_DAT *msg_data;	/* key schedule (return) */
	CREDENTIALS *cred;	/* local address */
	struct des_ks_struct *schedule;
	struct sockaddr_in *faddr; /* address of foreign host on fd */
	struct sockaddr_in *laddr;
	char *version;		/* version string */
{
    int rem, i, cc;
    char srv_inst[INST_SZ];
    char krb_realm[REALM_SZ];
    char buf[BUFSIZ];
    u_int32_t tkt_len;
    u_char priv_buf[1024];
    u_int32_t cksum;

    rem=KSUCCESS;

    /* get current realm if not passed in */
    if (!realm) {
	rem = krb_get_lrealm(krb_realm,1);
	if (rem != KSUCCESS)
	    return(rem);
	realm = krb_realm;
    }

    /* copy instance into local storage, canonicalizing if desired */
    if (options & KOPT_DONT_CANON)
	(void) strncpy(srv_inst, inst, INST_SZ);
    else
	(void) strncpy(srv_inst, krb_get_phost(inst), INST_SZ);

    /* get the ticket if desired */
    if (!(options & KOPT_DONT_MK_REQ)) {
	rem = krb_mk_req(ticket, service, srv_inst, realm, checksum);
	if (rem != KSUCCESS)
	    return(rem);
    }

#ifdef ATHENA_COMPAT
    /* this is only for compatibility with old servers */
    if (options & KOPT_DO_OLDSTYLE) {
	(void) snprintf(buf, sizeof(buf), "%d ", ticket->length);
	(void) write(fd, buf, strlen(buf));
	(void) write(fd, (char *) ticket->dat, ticket->length);
	return(rem);
    }
#endif /* ATHENA_COMPAT */
    /* if mutual auth, get credentials so we have service session
       keys for decryption below */
    if (options & KOPT_DO_MUTUAL)
	if ((cc = krb_get_cred(service, srv_inst, realm, cred)))
	    return(cc);

    /* zero the buffer */
    (void) bzero(buf, BUFSIZ);

    /* insert version strings */
    (void) strncpy(buf, KRB_SENDAUTH_VERS, KRB_SENDAUTH_VLEN);
    (void) strncpy(buf+KRB_SENDAUTH_VLEN, version, KRB_SENDAUTH_VLEN);

    /* increment past vers strings */
    i = 2*KRB_SENDAUTH_VLEN;

    /* put ticket length into buffer */
    tkt_len = htonl(ticket->length);
    (void) bcopy((char *) &tkt_len, buf+i, sizeof(tkt_len));
    i += sizeof(tkt_len);

    /* put ticket into buffer */
    (void) bcopy((char *) ticket->dat, buf+i, ticket->length);
    i += ticket->length;

    /* write the request to the server */
    if ((cc = krb_net_write(fd, buf, i)) != i)
	return(cc);

    /* mutual authentication, if desired */
    if (options & KOPT_DO_MUTUAL) {
	/* get the length of the reply */
	if (krb_net_read(fd, (char *) &tkt_len, sizeof(tkt_len)) !=
	    sizeof(tkt_len))
	    return(errno);
	tkt_len = ntohl(tkt_len);

	/* if the length is negative, the server failed to recognize us. */
	if ((tkt_len < 0) || (tkt_len > sizeof(priv_buf)))
	    return(KFAILURE);	 /* XXX */
	/* read the reply... */
	if (krb_net_read(fd, (char *)priv_buf, (int) tkt_len) != (int) tkt_len)
	    return(errno);

	/* ...and decrypt it */
#ifndef NOENCRYPTION
	des_key_sched(&cred->session,schedule);
#endif
	if ((cc = krb_rd_priv(priv_buf, tkt_len, schedule,
			     &cred->session, faddr, laddr, msg_data)))
	    return(cc);

	/* fetch the (modified) checksum */
	(void) bcopy((char *)msg_data->app_data, (char *)&cksum,
		     sizeof(cksum));
	cksum = ntohl(cksum);

	/* if it doesn't match, fail */
	if (cksum != checksum + 1)
	    return(KFAILURE);	 /* XXX */
    }
    return(KSUCCESS);
}

#ifdef ATHENA_COMPAT
/*
 * krb_sendsvc
 */

int
krb_sendsvc(fd, service)
	int fd;
	char *service;
{
    /* write the service name length and then the service name to
       the fd */
    u_int32_t serv_length;
    int cc;

    serv_length = htonl(strlen(service));
    if ((cc = krb_net_write(fd, (char *) &serv_length,
	sizeof(serv_length)))
	!= sizeof(serv_length))
	return(cc);
    if ((cc = krb_net_write(fd, service, strlen(service)))
	!= strlen(service))
	return(cc);
    return(KSUCCESS);
}
#endif /* ATHENA_COMPAT */
