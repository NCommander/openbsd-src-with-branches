/*	$OpenBSD: auth.c,v 1.3 1996/04/21 23:41:16 deraadt Exp $	*/

/*
 * auth.c - PPP authentication and phase control.
 *
 * Copyright (c) 1993 The Australian National University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Australian National University.  The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: auth.c,v 1.3 1996/04/21 23:41:16 deraadt Exp $";
#endif

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <pwd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef USE_PAM
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#endif

#ifdef HAS_SHADOW
#include <shadow.h>
#include <shadow/pwauth.h>
#ifndef PW_PPP
#define PW_PPP PW_LOGIN
#endif
#endif

#include "pppd.h"
#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#include "upap.h"
#include "chap.h"
#include "pathnames.h"

#if defined(sun) && defined(sparc)
#include <alloca.h>
#endif /*sparc*/

/* Used for storing a sequence of words.  Usually malloced. */
struct wordlist {
    struct wordlist	*next;
    char		word[1];
};

/* Bits in scan_authfile return value */
#define NONWILD_SERVER	1
#define NONWILD_CLIENT	2

#define ISWILD(word)	(word[0] == '*' && word[1] == 0)

#define FALSE	0
#define TRUE	1

/* The name by which the peer authenticated itself to us. */
char peer_authname[MAXNAMELEN];

/* Records which authentication operations haven't completed yet. */
static int auth_pending[NUM_PPP];

/* Set if we have successfully called login() */
static int logged_in;

/* Set if we have run the /etc/ppp/auth-up script. */
static int did_authup;

/* List of addresses which the peer may use. */
static struct wordlist *addresses[NUM_PPP];

/* Number of network protocols which we have opened. */
static int num_np_open;

/* Number of network protocols which have come up. */
static int num_np_up;

/* Set if we got the contents of passwd[] from the pap-secrets file. */
static int passwd_from_file;

/* Bits in auth_pending[] */
#define UPAP_WITHPEER	1
#define UPAP_PEER	2
#define CHAP_WITHPEER	4
#define CHAP_PEER	8

/* Prototypes for procedures local to this file. */

static void network_phase __P((int));
static void check_idle __P((caddr_t));
static int  login __P((char *, char *, char **, int *));
static void logout __P((void));
static int  null_login __P((int));
static int  get_upap_passwd __P((char *));
static int  have_upap_secret __P((void));
static int  have_chap_secret __P((char *, char *, u_int32_t));
static int  ip_addr_check __P((u_int32_t, struct wordlist *));
static int  scan_authfile __P((FILE *, char *, char *, u_int32_t, char *,
			       struct wordlist **, char *));
static void free_wordlist __P((struct wordlist *));
static void auth_script __P((char *));


/*
 * An Open on LCP has requested a change from Dead to Establish phase.
 * Do what's necessary to bring the physical layer up.
 */
void
link_required(unit)
    int unit;
{
}

/*
 * LCP has terminated the link; go to the Dead phase and take the
 * physical layer down.
 */
void
link_terminated(unit)
    int unit;
{
    if (phase == PHASE_DEAD)
	return;
    if (logged_in)
	logout();
    phase = PHASE_DEAD;
    syslog(LOG_NOTICE, "Connection terminated.");
}

/*
 * LCP has gone down; it will either die or try to re-establish.
 */
void
link_down(unit)
    int unit;
{
    int i;
    struct protent *protp;

    if (did_authup) {
        auth_script(_PATH_AUTHDOWN);
        did_authup = 0;
    }
    for (i = 0; (protp = protocols[i]) != NULL; ++i) {
	if (!protp->enabled_flag)
	    continue;
        if (protp->protocol != PPP_LCP && protp->lowerdown != NULL)
	    (*protp->lowerdown)(unit);
        if (protp->protocol < 0xC000 && protp->close != NULL)
	    (*protp->close)(unit, "LCP link down");

    }
    num_np_open = 0;
    num_np_up = 0;
    phase = PHASE_TERMINATE;
}

/*
 * The link is established.
 * Proceed to the Dead, Authenticate or Network phase as appropriate.
 */
void
link_established(unit)
    int unit;
{
    int auth;
    lcp_options *wo = &lcp_wantoptions[unit];
    lcp_options *go = &lcp_gotoptions[unit];
    lcp_options *ho = &lcp_hisoptions[unit];
    int i;
    struct protent *protp;

    /*
     * Tell higher-level protocols that LCP is up.
     */
    for (i = 0; (protp = protocols[i]) != NULL; ++i)
        if (protp->protocol != PPP_LCP && protp->enabled_flag
	    && protp->lowerup != NULL)
	    (*protp->lowerup)(unit);

    if (auth_required && !(go->neg_chap || go->neg_upap)) {
	/*
	 * We wanted the peer to authenticate itself, and it refused:
	 * treat it as though it authenticated with PAP using a username
	 * of "" and a password of "".  If that's not OK, boot it out.
	 */
	if (!wo->neg_upap || !null_login(unit)) {
	    syslog(LOG_WARNING, "peer refused to authenticate");
	    lcp_close(unit, "peer refused to authenticate");
	    phase = PHASE_TERMINATE;
	    return;
	}
    }

    phase = PHASE_AUTHENTICATE;
    auth = 0;
    if (go->neg_chap) {
	ChapAuthPeer(unit, our_name, go->chap_mdtype);
	auth |= CHAP_PEER;
    } else if (go->neg_upap) {
	upap_authpeer(unit);
	auth |= UPAP_PEER;
    }
    if (ho->neg_chap) {
	ChapAuthWithPeer(unit, our_name, ho->chap_mdtype);
	auth |= CHAP_WITHPEER;
    } else if (ho->neg_upap) {
        if (passwd[0] == 0) {
            passwd_from_file = 1;
            if (!get_upap_passwd(passwd))
                syslog(LOG_ERR, "No secret found for PAP login");
        }
	upap_authwithpeer(unit, user, passwd);
	auth |= UPAP_WITHPEER;
    }
    auth_pending[unit] = auth;

    if (!auth)
	network_phase(unit);
}

/*
 * Proceed to the network phase.
 */
static void
network_phase(unit)
    int unit;
{
    int i;
    struct protent *protp;
    lcp_options *go = &lcp_gotoptions[unit];

    /*
     * If the peer had to authenticate, run the auth-up script now.
     */
    if ((go->neg_chap || go->neg_upap) && !did_authup) {
      auth_script(_PATH_AUTHUP);
      did_authup = 1;
    }

    phase = PHASE_NETWORK;
#if 0
    if (!demand)
	set_filters(&pass_filter, &active_filter);
#endif
    for (i = 0; (protp = protocols[i]) != NULL; ++i)
        if (protp->protocol < 0xC000 && protp->enabled_flag
	    && protp->open != NULL) {
	    (*protp->open)(unit);
	    if (protp->protocol != PPP_CCP)
		++num_np_open;
	}
}

/*
 * The peer has failed to authenticate himself using `protocol'.
 */
void
auth_peer_fail(unit, protocol)
    int unit, protocol;
{
    /*
     * Authentication failure: take the link down
     */
    lcp_close(unit, "Authentication failed");
    phase = PHASE_TERMINATE;
}

/*
 * The peer has been successfully authenticated using `protocol'.
 */
void
auth_peer_success(unit, protocol, name, namelen)
    int unit, protocol;
    char *name;
    int namelen;
{
    int bit;

    switch (protocol) {
    case PPP_CHAP:
	bit = CHAP_PEER;
	break;
    case PPP_PAP:
	bit = UPAP_PEER;
	break;
    default:
	syslog(LOG_WARNING, "auth_peer_success: unknown protocol %x",
	       protocol);
	return;
    }

    /*
     * Save the authenticated name of the peer for later.
     */
    if (namelen > sizeof(peer_authname) - 1)
        namelen = sizeof(peer_authname) - 1;
    BCOPY(name, peer_authname, namelen);
    peer_authname[namelen] = 0;

    /*
     * If there is no more authentication still to be done,
     * proceed to the network phase.
     */
    if ((auth_pending[unit] &= ~bit) == 0)
        network_phase(unit);
}

/*
 * We have failed to authenticate ourselves to the peer using `protocol'.
 */
void
auth_withpeer_fail(unit, protocol)
    int unit, protocol;
{
    if (passwd_from_file)
        BZERO(passwd, MAXSECRETLEN);

    /*
     * We've failed to authenticate ourselves to our peer.
     * He'll probably take the link down, and there's not much
     * we can do except wait for that.
     */
}

/*
 * We have successfully authenticated ourselves with the peer using `protocol'.
 */
void
auth_withpeer_success(unit, protocol)
    int unit, protocol;
{
    int bit;

    switch (protocol) {
    case PPP_CHAP:
	bit = CHAP_WITHPEER;
	break;
    case PPP_PAP:
        if (passwd_from_file)
            BZERO(passwd, MAXSECRETLEN);
	bit = UPAP_WITHPEER;
	break;
    default:
	syslog(LOG_WARNING, "auth_peer_success: unknown protocol %x",
	       protocol);
	bit = 0;
    }

    /*
     * If there is no more authentication still being done,
     * proceed to the network phase.
     */
    if ((auth_pending[unit] &= ~bit) == 0)
	network_phase(unit);
}


/*
 * np_up - a network protocol has come up.
 */
void
np_up(unit, proto)
    int unit, proto;
{
    if (num_np_up == 0 && idle_time_limit > 0) {
	TIMEOUT(check_idle, NULL, idle_time_limit);
    }
    ++num_np_up;
}

/*
 * np_down - a network protocol has gone down.
 */
void
np_down(unit, proto)
    int unit, proto;
{
    if (--num_np_up == 0 && idle_time_limit > 0) {
	UNTIMEOUT(check_idle, NULL);
    }
}

/*
 * np_finished - a network protocol has finished using the link.
 */
void
np_finished(unit, proto)
    int unit, proto;
{
    if (--num_np_open <= 0) {
	/* no further use for the link: shut up shop. */
	lcp_close(0, "No network protocols running");
    }
}

/*
 * check_idle - check whether the link has been idle for long
 * enough that we can shut it down.
 */
static void
check_idle(arg)
    caddr_t arg;
{
    struct ppp_idle idle;
    time_t itime;

    if (!get_idle_time(0, &idle))
	return;
    itime = MIN(idle.xmit_idle, idle.recv_idle);
    if (itime >= idle_time_limit) {
	/* link is idle: shut it down. */
	syslog(LOG_INFO, "Terminating connection due to lack of activity.");
	lcp_close(0, "Link inactive");
    } else {
	TIMEOUT(check_idle, NULL, idle_time_limit - itime);
    }
}

/*
 * auth_check_options - called to check authentication options.
 */
void
auth_check_options()
{
    lcp_options *wo = &lcp_wantoptions[0];
    int can_auth;
    lcp_options *ao = &lcp_allowoptions[0];
    ipcp_options *ipwo = &ipcp_wantoptions[0];
    u_int32_t remote;

    /* Default our_name to hostname, and user to our_name */
    if (our_name[0] == 0 || usehostname)
	strcpy(our_name, hostname);
    if (user[0] == 0)
	strcpy(user, our_name);

    /* If authentication is required, ask peer for CHAP or PAP. */
    if (auth_required && !wo->neg_chap && !wo->neg_upap) {
	wo->neg_chap = 1;
	wo->neg_upap = 1;
    }

    /*
     * Check whether we have appropriate secrets to use
     * to authenticate ourselves and/or the peer.
     */
    if (ao->neg_upap && passwd[0] == 0 && !get_upap_passwd(passwd))
	ao->neg_upap = 0;
    if (wo->neg_upap && !uselogin && !have_upap_secret())
	wo->neg_upap = 0;
    if (ao->neg_chap && !have_chap_secret(our_name, remote_name, (u_int32_t)0))
	ao->neg_chap = 0;
    if (wo->neg_chap) {
	remote = ipwo->accept_remote? 0: ipwo->hisaddr;
	if (!have_chap_secret(remote_name, our_name, remote))
	    wo->neg_chap = 0;
    }

    if (auth_required && !wo->neg_chap && !wo->neg_upap) {
	fprintf(stderr, "\
pppd: peer authentication required but no suitable secret(s) found\n");
	exit(1);
    }

}

/*
 * auth_reset - called when LCP is starting negotiations to recheck
 * authentication options, i.e. whether we have appropriate secrets
 * to use for authenticating ourselves and/or the peer.
 */
void
auth_reset(unit)
    int unit;
{
    lcp_options *go = &lcp_gotoptions[unit];
    lcp_options *ao = &lcp_allowoptions[0];
    ipcp_options *ipwo = &ipcp_wantoptions[0];
    u_int32_t remote;
    
    ao->neg_upap = !refuse_pap && (passwd[0] != 0 || get_upap_passwd(NULL));
    ao->neg_chap = !refuse_chap
        && have_chap_secret(our_name, remote_name, (u_int32_t)0);
    
    if (go->neg_upap && !uselogin && !have_upap_secret())
        go->neg_upap = 0;
    if (go->neg_chap) {
        remote = ipwo->accept_remote? 0: ipwo->hisaddr;
        if (!have_chap_secret(remote_name, our_name, remote))
            go->neg_chap = 0;
    }

}


/*
 * check_passwd - Check the user name and passwd against the PAP secrets
 * file.  If requested, also check against the system password database,
 * and login the user if OK.
 *
 * returns:
 *	UPAP_AUTHNAK: Authentication failed.
 *	UPAP_AUTHACK: Authentication succeeded.
 * In either case, msg points to an appropriate message.
 */
int
check_passwd(unit, auser, userlen, apasswd, passwdlen, msg, msglen)
    int unit;
    char *auser;
    int userlen;
    char *apasswd;
    int passwdlen;
    char **msg;
    int *msglen;
{
    int ret;
    char *filename;
    FILE *f;
    struct wordlist *addrs;
    u_int32_t remote;
    ipcp_options *ipwo = &ipcp_wantoptions[unit];
    char passwd[256], user[256];
    char secret[MAXWORDLEN];
    static int attempts = 0;

    /*
     * Make copies of apasswd and auser, then null-terminate them.
     */
    BCOPY(apasswd, passwd, passwdlen);
    passwd[passwdlen] = '\0';
    BCOPY(auser, user, userlen);
    user[userlen] = '\0';
    *msg = (char *) 0;


    /*
     * Open the file of upap secrets and scan for a suitable secret
     * for authenticating this user.
     */
    filename = _PATH_UPAPFILE;
    addrs = NULL;
    ret = UPAP_AUTHACK;
    f = fopen(filename, "r");
    if (f == NULL) {
	if (!uselogin) {
	    syslog(LOG_ERR, "Can't open PAP password file %s: %m", filename);
	    ret = UPAP_AUTHNAK;
	}

    } else {
	check_access(f, filename);
	remote = ipwo->accept_remote? 0: ipwo->hisaddr;
	if (scan_authfile(f, user, our_name, remote,
			  secret, &addrs, filename) < 0
	    || (secret[0] != 0 && (cryptpap || strcmp(passwd, secret) != 0)
		&& strcmp(crypt(passwd, secret), secret) != 0)) {
	    syslog(LOG_WARNING, "PAP authentication failure for %s", user);
	    ret = UPAP_AUTHNAK;
	}
	fclose(f);
    }

    if (uselogin && ret == UPAP_AUTHACK) {
	ret = login(user, passwd, msg, msglen);
	if (ret == UPAP_AUTHNAK) {
	    syslog(LOG_WARNING, "PAP login failure for %s", user);
	}
    }

    if (ret == UPAP_AUTHNAK) {
        if (*msg == (char *) 0)
	    *msg = "Login incorrect";
	*msglen = strlen(*msg);
	/*
	 * Frustrate passwd stealer programs.
	 * Allow 10 tries, but start backing off after 3 (stolen from login).
	 * On 10'th, drop the connection.
	 */
	if (attempts++ >= 10) {
	    syslog(LOG_WARNING, "%d LOGIN FAILURES ON %s, %s",
		   attempts, devnam, user);
	    quit();
	}
	if (attempts > 3)
	    sleep((u_int) (attempts - 3) * 5);
	if (addrs != NULL)
	    free_wordlist(addrs);

    } else {
	attempts = 0;			/* Reset count */
        if (*msg == (char *) 0)
	    *msg = "Login ok";
	*msglen = strlen(*msg);
	if (addresses[unit] != NULL)
	    free_wordlist(addresses[unit]);
	addresses[unit] = addrs;
    }

    BZERO(passwd, sizeof(passwd));   
    BZERO(secret, sizeof(secret));

    return ret;
}

#ifdef HAS_SHADOW
/**************
 * This function was lifted from the shadow-3.3.2 version by John Haugh II.
 * It is included because the function was not in the standard libshadow
 * library. If it is included in the library then I can remove it from here.
 */
        
#define DAY     (24L*3600L)
/*
 * isexpired - determine if account is expired yet
 *
 *      isexpired calculates the expiration date based on the
 *      password expiration criteria.
 */
           
/*ARGSUSED*/
int
isexpired (pw, sp)
struct  passwd  *pw;
struct  spwd    *sp;
{
        long    clock;
 
        clock = time ((time_t *) 0) / DAY;
   
        /*
         * Quick and easy - there is an expired account field
         * along with an inactive account field.  Do the expired
         * one first since it is worse.
         */
 
        if (sp->sp_expire > 0 && sp->sp_expire < clock)
                return 3;

        if (sp->sp_inact > 0 && sp->sp_lstchg > 0 && sp->sp_max > 0 &&
                        sp->sp_inact + sp->sp_lstchg + sp->sp_max < clock)
                return 2;
 
        /*
         * The last and max fields must be present for an account
         * to have an expired password.  A maximum of >10000 days
         * is considered to be infinite.
         */

        if (sp->sp_lstchg == -1 ||
                        sp->sp_max == -1 || sp->sp_max >= 10000L)
                return 0;
        
        /*
         * Calculate today's day and the day on which the password
         * is going to expire.  If that date has already passed,
         * the password has expired.
         */
         
        if (sp->sp_lstchg + sp->sp_max < clock)
                return 1;
 
        return 0;
}
#endif

/*
 * This function is needed for PAM. However, it should not be called.
 * If it is, return the error code.
 */
         
#ifdef USE_PAM
static int pam_conv(int num_msg, const struct pam_message **msg,
                    struct pam_response **resp, void *appdata_ptr)
{
    return PAM_CONV_ERR;
}
#endif  


/*
 * login - Check the user name and password against the system
 * password database, and login the user if OK.
 *
 * returns:
 *	UPAP_AUTHNAK: Login failed.
 *	UPAP_AUTHACK: Login succeeded.
 * In either case, msg points to an appropriate message.
 */

static int
login(user, passwd, msg, msglen)
    char *user;
    char *passwd;
    char **msg;
    int *msglen;
{
    char *tty;

#ifdef USE_PAM
    struct pam_conv pam_conversation;
    pam_handle_t *pamh;
    int pam_error;
    char *pass;
    char *dev;
/*
 * Fill the pam_conversion structure
 */
    memset (&pam_conversation, '\0', sizeof (struct pam_conv));
    pam_conversation.conv = &pam_conv;
    
    pam_error = pam_start ("ppp", user, &pam_conversation, &pamh);
    if (pam_error != PAM_SUCCESS) {
        *msg = (char *) pam_strerror (pam_error);
        return UPAP_AUTHNAK;
    }
/*        
 * Define the fields for the credintial validation
 */
    (void) pam_set_item (pamh, PAM_AUTHTOK, passwd);
    (void) pam_set_item (pamh, PAM_TTY, devnam);
/*
 * Validate the user
 */
    pam_error = pam_authenticate (pamh, PAM_SILENT);
    if (pam_error == PAM_SUCCESS)
        pam_error = pam_acct_mgmt (pamh, PAM_SILENT);
    
    *msg = (char *) pam_strerror (pam_error);
/*
 * Clean up the mess
 */
    (void) pam_end (pamh, pam_error);
   
    if (pam_error != PAM_SUCCESS)
        return UPAP_AUTHNAK;
/*  
 * Use the non-PAM methods directly
 */
#else /* #ifdef USE_PAM */
        
    struct passwd *pw;
    char *epasswd;
     
#ifdef HAS_SHADOW
    struct spwd *spwd;
    struct spwd *getspnam();
#endif
       
    if ((pw = getpwnam(user)) == NULL) {
        return (UPAP_AUTHNAK);
    }   
    
#ifdef HAS_SHADOW 
    if ((spwd = getspnam(user)) == NULL) {   
        pw->pw_passwd = "";
    } else {
        pw->pw_passwd = spwd->sp_pwdp;
    }
#endif
    
    /*
     * XXX If no passwd, let them login without one.
     */
    if (pw->pw_passwd == '\0') {
        return (UPAP_AUTHACK);
    }   
    
#ifdef HAS_SHADOW 
    if (pw->pw_passwd) {
        if (pw->pw_passwd[0] == '@') {
            if (pw_auth (pw->pw_passwd+1, pw->pw_name, PW_PPP, NULL)) {
                return (UPAP_AUTHNAK);
            }
        } else {
            epasswd = pw_encrypt(passwd, pw->pw_passwd);
            if (strcmp(epasswd, pw->pw_passwd)) {
                return (UPAP_AUTHNAK);
            }
        }
        /* check the age of the password entry */
        if (spwd && (isexpired (pw, spwd) != 0)) {
            return (UPAP_AUTHNAK);
        }
    }
#else
    epasswd = crypt(passwd, pw->pw_passwd);
    if (strcmp(epasswd, pw->pw_passwd)) {
        return (UPAP_AUTHNAK);
    }
#endif
#endif /* #ifdef USE_PAM */   

    syslog(LOG_INFO, "user %s logged in", user);

    /*
     * Write a wtmp entry for this user.
     */
    tty = devnam;
    if (strncmp(tty, "/dev/", 5) == 0)
	tty += 5;
    logwtmp(tty, user, remote_name);		/* Add wtmp login entry */
    logged_in = TRUE;

    return (UPAP_AUTHACK);
}

/*
 * logout - Logout the user.
 */
static void
logout()
{
    char *tty;

    tty = devnam;
    if (strncmp(tty, "/dev/", 5) == 0)
	tty += 5;
    logwtmp(tty, "", "");		/* Wipe out wtmp logout entry */
    logged_in = FALSE;
}


/*
 * null_login - Check if a username of "" and a password of "" are
 * acceptable, and iff so, set the list of acceptable IP addresses
 * and return 1.
 */
static int
null_login(unit)
    int unit;
{
    char *filename;
    FILE *f;
    int i, ret;
    struct wordlist *addrs;
    char secret[MAXWORDLEN];

    /*
     * Open the file of upap secrets and scan for a suitable secret.
     * We don't accept a wildcard client.
     */
    filename = _PATH_UPAPFILE;
    addrs = NULL;
    f = fopen(filename, "r");
    if (f == NULL)
	return 0;
    check_access(f, filename);

    i = scan_authfile(f, "", our_name, (u_int32_t)0, secret, &addrs, filename);
    ret = i >= 0 && (i & NONWILD_CLIENT) != 0 && secret[0] == 0;
    BZERO(secret, sizeof(secret));

    if (ret) {
	if (addresses[unit] != NULL)
	    free_wordlist(addresses[unit]);
	addresses[unit] = addrs;
    }

    fclose(f);
    return ret;
}


/*
 * get_upap_passwd - get a password for authenticating ourselves with
 * our peer using PAP.  Returns 1 on success, 0 if no suitable password
 * could be found.
 */
static int
get_upap_passwd(passwd)
char *passwd;
{
    char *filename;
    FILE *f;
    struct wordlist *addrs;
    char secret[MAXWORDLEN];

    filename = _PATH_UPAPFILE;
    addrs = NULL;
    f = fopen(filename, "r");
    if (f == NULL)
	return 0;
    check_access(f, filename);
    if (scan_authfile(f, user,
                      remote_name[0]? remote_name: NULL,
                      (u_int32_t)0, secret, NULL, filename) < 0)
	return 0;
    if (passwd != NULL) {
        strncpy(passwd, secret, MAXSECRETLEN);
        passwd[MAXSECRETLEN-1] = 0;
    }   
    BZERO(secret, sizeof(secret));
    return 1;
}


/*
 * have_upap_secret - check whether we have a PAP file with any
 * secrets that we could possibly use for authenticating the peer.
 */
static int
have_upap_secret()
{
    FILE *f;
    int ret;
    char *filename;
    ipcp_options *ipwo = &ipcp_wantoptions[0];
    u_int32_t remote;

    filename = _PATH_UPAPFILE;
    f = fopen(filename, "r");
    if (f == NULL)
	return 0;

    remote = ipwo->accept_remote? 0: ipwo->hisaddr;
    ret = scan_authfile(f, NULL, our_name, remote, NULL, NULL, filename);
    fclose(f);
    if (ret < 0)
	return 0;

    return 1;
}


/*
 * have_chap_secret - check whether we have a CHAP file with a
 * secret that we could possibly use for authenticating `client'
 * on `server'.  Either can be the null string, meaning we don't
 * know the identity yet.
 */
static int
have_chap_secret(client, server, remote)
    char *client;
    char *server;
    u_int32_t remote;
{
    FILE *f;
    int ret;
    char *filename;

    filename = _PATH_CHAPFILE;
    f = fopen(filename, "r");
    if (f == NULL)
	return 0;

    if (client[0] == 0)
	client = NULL;
    else if (server[0] == 0)
	server = NULL;

    ret = scan_authfile(f, client, server, remote, NULL, NULL, filename);
    fclose(f);
    if (ret < 0)
	return 0;

    return 1;
}


/*
 * get_secret - open the CHAP secret file and return the secret
 * for authenticating the given client on the given server.
 * (We could be either client or server).
 */
int
get_secret(unit, client, server, secret, secret_len, save_addrs)
    int unit;
    char *client;
    char *server;
    char *secret;
    int *secret_len;
    int save_addrs;
{
    FILE *f;
    int ret, len;
    char *filename;
    struct wordlist *addrs;
    char secbuf[MAXWORDLEN];

    filename = _PATH_CHAPFILE;
    addrs = NULL;
    secbuf[0] = 0;

    f = fopen(filename, "r");
    if (f == NULL) {
	syslog(LOG_ERR, "Can't open chap secret file %s: %m", filename);
	return 0;
    }
    check_access(f, filename);

    ret = scan_authfile(f, client, server, (u_int32_t)0,
			secbuf, &addrs, filename);
    fclose(f);
    if (ret < 0)
	return 0;

    if (save_addrs) {
	if (addresses[unit] != NULL)
	    free_wordlist(addresses[unit]);
	addresses[unit] = addrs;
    }

    len = strlen(secbuf);
    if (len > MAXSECRETLEN) {
	syslog(LOG_ERR, "Secret for %s on %s is too long", client, server);
	len = MAXSECRETLEN;
    }
    BCOPY(secbuf, secret, len);
    *secret_len = len;

    return 1;
}

/*
 * auth_ip_addr - check whether the peer is authorized to use
 * a given IP address.  Returns 1 if authorized, 0 otherwise.
 */
int
auth_ip_addr(unit, addr)
    int unit;
    u_int32_t addr;
{
    return ip_addr_check(addr, addresses[unit]);
}

static int
ip_addr_check(addr, addrs)
    u_int32_t addr;
    struct wordlist *addrs;
{
    u_int32_t a, mask, ah;
    int accept;
    char *ptr_word, *ptr_mask;
    struct hostent *hp;
    struct netent *np;

    /* don't allow loopback or multicast address */
    if (bad_ip_adrs(addr))
	return 0;

    if (addrs == NULL)
	return 1;		/* no restriction */

    for (; addrs != NULL; addrs = addrs->next) {
	/* "-" means no addresses authorized */
	ptr_word = addrs->word;
	if (strcmp(ptr_word, "-") == 0)
	    break;

	accept = 1;
	if (*ptr_word == '!') {
	    accept = 0;
	    ++ptr_word;
	}

	mask = ~ (u_int32_t) 0;
	ptr_mask = strchr (ptr_word, '/');
	if (ptr_mask != NULL) {
	    int bit_count;

	    bit_count = (int) strtol (ptr_mask+1, (char **) 0, 10);
	    if (bit_count <= 0 || bit_count > 32) {
		syslog (LOG_WARNING,
			"invalid address length %s in auth. address list",
			ptr_mask);
		continue;
	    }
	    *ptr_mask = '\0';
	    mask <<= 32 - bit_count;
	}

	hp = gethostbyname(ptr_word);
	if (hp != NULL && hp->h_addrtype == AF_INET) {
	    a    = *(u_int32_t *)hp->h_addr;
	    mask = ~ (u_int32_t) 0;	/* are we sure we want this? */
	} else {
	    np = getnetbyname (ptr_word);
	    if (np != NULL && np->n_addrtype == AF_INET)
		a = htonl (*(u_int32_t *)np->n_net);
	    else
		a = inet_addr (ptr_word);
	    if (ptr_mask == NULL) {
		/* calculate appropriate mask for net */
		ah = ntohl(a);
		if (IN_CLASSA(ah))
		    mask = IN_CLASSA_NET;
		else if (IN_CLASSB(ah))
		    mask = IN_CLASSB_NET;
		else if (IN_CLASSC(ah))
		    mask = IN_CLASSC_NET;
	    }
	}

	if (ptr_mask != NULL)
	    *ptr_mask = '/';

        if (a == -1L)
	    syslog (LOG_WARNING,
		    "unknown host %s in auth. address list",
		    addrs->word);
	else
	    if (((addr ^ a) & htonl(mask)) == 0)
		return accept;
    }
    return 0;			/* not in list => can't have it */
}

/*
 * bad_ip_adrs - return 1 if the IP address is one we don't want
 * to use, such as an address in the loopback net or a multicast address.
 * addr is in network byte order.
 */
int
bad_ip_adrs(addr)
    u_int32_t addr;
{
    addr = ntohl(addr);
    return (addr >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET
	|| IN_MULTICAST(addr) || IN_BADCLASS(addr);
}

/*
 * check_access - complain if a secret file has too-liberal permissions.
 */
void
check_access(f, filename)
    FILE *f;
    char *filename;
{
    struct stat sbuf;

    if (fstat(fileno(f), &sbuf) < 0) {
	syslog(LOG_WARNING, "cannot stat secret file %s: %m", filename);
    } else if ((sbuf.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
	syslog(LOG_WARNING, "Warning - secret file %s has world and/or group access", filename);
    }
}


/*
 * scan_authfile - Scan an authorization file for a secret suitable
 * for authenticating `client' on `server'.  The return value is -1
 * if no secret is found, otherwise >= 0.  The return value has
 * NONWILD_CLIENT set if the secret didn't have "*" for the client, and
 * NONWILD_SERVER set if the secret didn't have "*" for the server.
 * Any following words on the line (i.e. address authorization
 * info) are placed in a wordlist and returned in *addrs.  
 */
static int
scan_authfile(f, client, server, ipaddr, secret, addrs, filename)
    FILE *f;
    char *client;
    char *server;
    u_int32_t ipaddr;
    char *secret;
    struct wordlist **addrs;
    char *filename;
{
    int newline, xxx;
    int got_flag, best_flag;
    FILE *sf;
    struct wordlist *ap, *addr_list, *alist, *alast;
    char word[MAXWORDLEN];
    char atfile[MAXWORDLEN];
    char lsecret[MAXWORDLEN];

    if (addrs != NULL)
	*addrs = NULL;
    addr_list = NULL;
    if (!getword(f, word, &newline, filename))
	return -1;		/* file is empty??? */
    newline = 1;
    best_flag = -1;
    for (;;) {
	/*
	 * Skip until we find a word at the start of a line.
	 */
	while (!newline && getword(f, word, &newline, filename))
	    ;
	if (!newline)
	    break;		/* got to end of file */

	/*
	 * Got a client - check if it's a match or a wildcard.
	 */
	got_flag = 0;
	if (client != NULL && strcmp(word, client) != 0 && !ISWILD(word)) {
	    newline = 0;
	    continue;
	}
	if (!ISWILD(word))
	    got_flag = NONWILD_CLIENT;

	/*
	 * Now get a server and check if it matches.
	 */
	if (!getword(f, word, &newline, filename))
	    break;
	if (newline)
	    continue;
	if (server != NULL && strcmp(word, server) != 0 && !ISWILD(word))
	    continue;
	if (!ISWILD(word))
	    got_flag |= NONWILD_SERVER;

	/*
	 * Got some sort of a match - see if it's better than what
	 * we have already.
	 */
	if (got_flag <= best_flag)
	    continue;

	/*
	 * Get the secret.
	 */
	if (!getword(f, word, &newline, filename))
	    break;
	if (newline)
	    continue;

	/*
	 * Special syntax: @filename means read secret from file.
	 */
	if (word[0] == '@') {
	    strcpy(atfile, word+1);
	    if ((sf = fopen(atfile, "r")) == NULL) {
		syslog(LOG_WARNING, "can't open indirect secret file %s",
		       atfile);
		continue;
	    }
	    check_access(sf, atfile);
	    if (!getword(sf, word, &xxx, atfile)) {
		syslog(LOG_WARNING, "no secret in indirect secret file %s",
		       atfile);
		fclose(sf);
		continue;
	    }
	    fclose(sf);
	}
	if (secret != NULL)
	    strcpy(lsecret, word);

	/*
	 * Now read address authorization info and make a wordlist.
	 */
	alist = alast = NULL;
	for (;;) {
	    if (!getword(f, word, &newline, filename) || newline)
		break;
	    ap = (struct wordlist *) malloc(sizeof(struct wordlist)
					    + strlen(word));
	    if (ap == NULL)
		novm("authorized addresses");
	    ap->next = NULL;
	    strcpy(ap->word, word);
	    if (alist == NULL)
		alist = ap;
	    else
		alast->next = ap;
	    alast = ap;
	}

	/*
	 * Check if the given IP address is allowed by the wordlist.
	 */
	if (ipaddr != 0 && !ip_addr_check(ipaddr, alist)) {
	    free_wordlist(alist);
	    continue;
	}

	/*
	 * This is the best so far; remember it.
	 */
	best_flag = got_flag;
	if (addr_list)
	    free_wordlist(addr_list);
	addr_list = alist;
	if (secret != NULL)
	    strcpy(secret, lsecret);

	if (!newline)
	    break;
    }

    if (addrs != NULL)
	*addrs = addr_list;
    else if (addr_list != NULL)
	free_wordlist(addr_list);

    return best_flag;
}

/*
 * free_wordlist - release memory allocated for a wordlist.
 */
static void
free_wordlist(wp)
    struct wordlist *wp;
{
    struct wordlist *next;

    while (wp != NULL) {
	next = wp->next;
	free(wp);
	wp = next;
    }
}

/*
 * auth_script - execute a script with arguments
 * interface-name peer-name real-user tty speed
 */
static void
auth_script(script)
    char *script;
{
    char strspeed[32];
    struct passwd *pw;
    char struid[32];
    char *user_name; 
    char *argv[8];
    
    if ((pw = getpwuid(getuid())) != NULL && pw->pw_name != NULL)
        user_name = pw->pw_name; 
    else {
        sprintf(struid, "%d", getuid());
        user_name = struid;
    }
    sprintf(strspeed, "%d", baud_rate);
        
    argv[0] = script;
    argv[1] = ifname;
    argv[2] = peer_authname;
    argv[3] = user_name;
    argv[4] = devnam;
    argv[5] = strspeed;
    argv[6] = NULL;
   
    run_program(script, argv, 0);
}

