/*	$Id: kerberos.c,v 1.2 1996/09/16 18:48:54 millert Exp $	*/

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

#include "kerberosIV/site.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>

#include <sys/time.h>
#include <time.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <netdb.h>

#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>
#include <kerberosIV/krb_db.h>

#include <prot.h>
#include <klog.h>
#include <kdc.h>

static struct sockaddr_in sina = {AF_INET};
int     f;

/* XXX several files in libkdb know about this */
char *progname;

static des_key_schedule master_key_schedule;
static des_cblock master_key;

static struct timeval kerb_time;
static Principal a_name_data;	/* for requesting user */
static Principal s_name_data;	/* for services requested */
static des_cblock session_key;
static u_char master_key_version;
static char k_instance[INST_SZ];
static char *lt;
static int more;

static int mflag;		/* Are we invoked manually? */
static int lflag;		/* Have we set an alterate log file? */
static char *log_file;		/* name of alt. log file */
static int nflag;		/* don't check max age */
static int rflag;		/* alternate realm specified */

/* fields within the received request packet */
static u_char req_msg_type;
static u_char req_version;
static char *req_name_ptr;
static char *req_inst_ptr;
static char *req_realm_ptr;
static u_long req_time_ws;

int req_act_vno = KRB_PROT_VERSION; /* Temporary for version skew */

static char local_realm[REALM_SZ];

/* statistics */
static int q_bytes;		/* current bytes remaining in queue */
static int q_n;		/* how many consecutive non-zero
				 * q_bytes   */
static int max_q_bytes;
static int max_q_n;
static int n_auth_req;
static int n_appl_req;
static int n_packets;

static int max_age = -1;
static int pause_int = -1;

/*
 * Print usage message and exit.
 */
static void
usage(void)
{
    fprintf(stderr, "Usage: %s [-s] [-m] [-n] [-p pause_seconds]%s%s\n", progname, 
	    " [-a max_age] [-l log_file] [-r realm]"
	    ," [database_pathname]"
	    );
    exit(1);
}

/*
 * kerb_er_reply creates an error reply packet and sends it to the
 * client. 
 */

static void
kerb_err_reply(struct sockaddr_in *client, KTEXT pkt, long int err, char *string)
{
    static KTEXT_ST e_pkt_st;
    KTEXT   e_pkt = &e_pkt_st;
    static char e_msg[128];

    bzero(e_msg, sizeof e_msg);
    strcpy(e_msg, "\nKerberos error -- ");
    strcat(e_msg, string);
    cr_err_reply(e_pkt, req_name_ptr, req_inst_ptr, req_realm_ptr,
		 req_time_ws, err, e_msg);
    sendto(f, e_pkt->dat, e_pkt->length, 0, (struct sockaddr*)client, S_AD_SZ);
}

static void
hang(void)
{
    if (pause_int == -1) {
	klog(L_KRB_PERR, "Kerberos will pause so as not to loop init");
	for (;;)
	    pause();
    } else {
	char buf[256];
	(void) snprintf(buf, sizeof(buf),
	    "Kerberos will wait %d seconds before dying so as not to loop init",
	    pause_int);
	klog(L_KRB_PERR, buf);
	sleep(pause_int);
	klog(L_KRB_PERR, "Do svedania....\n");
	exit(1);
    }
}

/*
 * Given a pointer to a long containing the number of seconds
 * since the beginning of time (midnight 1 Jan 1970 GMT), return
 * a string containing the local time in the form:
 *
 * "25-Jan-88 10:17:56"
 */

static char *
strtime(time_t *t)
{
    static char st_data[40];
    static char *st = st_data;
    struct tm *tm;
    char *month_sname(int n);

    tm = localtime(t);
    (void) snprintf(st, sizeof(st_data), "%2d-%s-%02d %02d:%02d:%02d",
		    tm->tm_mday, month_sname(tm->tm_mon + 1), tm->tm_year,
                    tm->tm_hour, tm->tm_min, tm->tm_sec);
    return st;
}

static int
check_princ(char *p_name, char *instance, unsigned int lifetime, Principal *p)
{
    static int n;
    static int more;

    n = kerb_get_principal(p_name, instance, p, 1, &more);
    klog(L_ALL_REQ,
	 "Principal: \"%s\", Instance: \"%s\" Lifetime = %d n = %d",
	 p_name, instance, lifetime, n, 0);
    
    if (n < 0) {
	lt = klog(L_KRB_PERR, "Database unavailable!");
	hang();
    }
    
    /*
     * if more than one p_name, pick one, randomly create a session key,
     * compute maximum lifetime, lookup authorizations if applicable,
     * and stuff into cipher. 
     */
    if (n == 0) {
	/* service unknown, log error, skip to next request */
	lt = klog(L_ERR_UNK, "UNKNOWN \"%s\" \"%s\"", p_name,
	    instance, 0);
	return KERB_ERR_PRINCIPAL_UNKNOWN;
    }
    if (more) {
	/* not unique, log error */
	lt = klog(L_ERR_NUN, "Principal NOT UNIQUE \"%s\" \"%s\"",
		  p_name, instance, 0);
	return KERB_ERR_PRINCIPAL_NOT_UNIQUE;
    }
    /* If the user's key is null, we want to return an error */
    if ((p->key_low == 0) && (p->key_high == 0)) {
	/* User has a null key */
	lt = klog(L_ERR_NKY, "Null key \"%s\" \"%s\"", p_name,
	    instance, 0);
	return KERB_ERR_NULL_KEY;
    }
    if (master_key_version != p->kdc_key_ver) {
	/* log error reply */
	lt = klog(L_ERR_MKV,
	    "Key vers incorrect, KRB = %d, \"%s\" \"%s\" = %d",
	    master_key_version, p->name, p->instance, p->kdc_key_ver,
	    0);
	return KERB_ERR_NAME_MAST_KEY_VER;
    }
    /* make sure the service hasn't expired */
    if ((u_long) p->exp_date < (u_long) kerb_time.tv_sec) {
	/* service did expire, log it */
	lt = klog(L_ERR_SEXP,
	    "EXPIRED \"%s\" \"%s\"  %s", p->name, p->instance,
	     strtime((time_t*)&(p->exp_date)), 0);
	return KERB_ERR_NAME_EXP;
    }
    /* ok is zero */
    return 0;
}

/* Set the key for krb_rd_req so we can check tgt */
static int
set_tgtkey(char *r)
              			/* Realm for desired key */
{
    int     n;
    static char lastrealm[REALM_SZ];
    Principal p_st;
    Principal *p = &p_st;
    des_cblock key;

    if (!strcmp(lastrealm, r))
	return (KSUCCESS);

    log("Getting key for %s", r);

    n = kerb_get_principal("krbtgt", r, p, 1, &more);
    if (n == 0)
	return (KFAILURE);

    /* unseal tgt key from master key */
    bcopy(&p->key_low, key, 4);
    bcopy(&p->key_high, ((long *) key) + 1, 4);
    kdb_encrypt_key(&key, &key, &master_key,
		    master_key_schedule, DES_DECRYPT);
    krb_set_key(key, 0);
    strcpy(lastrealm, r);
    return (KSUCCESS);
}

static void
kerberos(struct sockaddr_in *client, KTEXT pkt)
{
    static KTEXT_ST rpkt_st;
    KTEXT   rpkt = &rpkt_st;
    static KTEXT_ST ciph_st;
    KTEXT   ciph = &ciph_st;
    static KTEXT_ST tk_st;
    KTEXT   tk = &tk_st;
    static KTEXT_ST auth_st;
    KTEXT   auth = &auth_st;
    AUTH_DAT ad_st;
    AUTH_DAT *ad = &ad_st;


    static struct in_addr client_host;
    static int msg_byte_order;
    static int swap_bytes;
    static u_char k_flags;
    u_long  lifetime;
    int     i;
    des_cblock key;
    des_key_schedule key_s;
    char   *ptr;



    ciph->length = 0;

    client_host = client->sin_addr;

    /* eval macros and correct the byte order and alignment as needed */
    req_version = pkt_version(pkt);	/* 1 byte, version */
    req_msg_type = pkt_msg_type(pkt);	/* 1 byte, Kerberos msg type */

    req_act_vno = req_version;

    /* check packet version */
    if (req_version != KRB_PROT_VERSION) {
	lt = klog(L_KRB_PERR,
	"KRB prot version mismatch: KRB =%d request = %d",
		  KRB_PROT_VERSION, req_version, 0);
	/* send an error reply */
	kerb_err_reply(client, pkt, KERB_ERR_PKT_VER, lt);
	return;
    }
    msg_byte_order = req_msg_type & 1;

    swap_bytes = 0;
    if (msg_byte_order != HOST_BYTE_ORDER) {
	swap_bytes++;
    }
    klog(L_KRB_PINFO,
	"Prot version: %d, Byte order: %d, Message type: %d",
	 req_version, msg_byte_order, req_msg_type);

    switch (req_msg_type & ~1) {

    case AUTH_MSG_KDC_REQUEST:
	{
	    u_long  req_life;	/* Requested liftime */
	    char   *service;	/* Service name */
	    char   *instance;	/* Service instance */

	    n_auth_req++;
	    tk->length = 0;
	    k_flags = 0;	/* various kerberos flags */


	    /* set up and correct for byte order and alignment */
	    req_name_ptr = (char *) pkt_a_name(pkt);
	    req_inst_ptr = (char *) pkt_a_inst(pkt);
	    req_realm_ptr = (char *) pkt_a_realm(pkt);
	    bcopy(pkt_time_ws(pkt), &req_time_ws, sizeof(req_time_ws));
	    /* time has to be diddled */
	    if (swap_bytes) {
		swap_u_long(req_time_ws);
	    }
	    ptr = (char *) pkt_time_ws(pkt) + 4;

	    req_life = (unsigned char) (*ptr++);

	    service = ptr;
	    instance = ptr + strlen(service) + 1;

	    rpkt = &rpkt_st;
	    klog(L_INI_REQ,
	    "Initial ticket request Host: %s User: \"%s\" \"%s\"",
	       inet_ntoa(client_host), req_name_ptr, req_inst_ptr, 0);

	    if ((i = check_princ(req_name_ptr, req_inst_ptr, 0,
		&a_name_data))) {
		kerb_err_reply(client, pkt, i, lt);
		return;
	    }
	    tk->length = 0;	/* init */
	    if (strcmp(service, "krbtgt"))
		klog(L_NTGT_INTK,
		    "INITIAL request from %s.%s for %s.%s",
		     req_name_ptr, req_inst_ptr, service, instance, 0);
	    /* this does all the checking */
	    if ((i = check_princ(service, instance, 0,
		&s_name_data))) {
		kerb_err_reply(client, pkt, i, lt);
		return;
	    }
	    /* Bound requested lifetime with service and user */
	    lifetime = min(req_life, ((u_long) s_name_data.max_life));
	    lifetime = min(lifetime, ((u_long) a_name_data.max_life));

#ifdef NOENCRYPTION
	    bzero(session_key, sizeof(des_cblock));
#else
	    des_new_random_key(&session_key);
#endif
	    /* unseal server's key from master key */
	    bcopy(&s_name_data.key_low, key, 4);
	    bcopy(&s_name_data.key_high, ((long *) key) + 1, 4);
	    kdb_encrypt_key(&key, &key, &master_key,
			    master_key_schedule, DES_DECRYPT);
	    /* construct and seal the ticket */
	    krb_create_ticket(tk, k_flags, a_name_data.name,
		a_name_data.instance, local_realm,
		 client_host.s_addr, session_key, lifetime, kerb_time.tv_sec,
			 s_name_data.name, s_name_data.instance, &key);
	    bzero(key, sizeof(key));
	    bzero(key_s, sizeof(key_s));

	    /*
	     * get the user's key, unseal it from the server's key, and
	     * use it to seal the cipher 
	     */

	    /* a_name_data.key_low a_name_data.key_high */
	    bcopy(&a_name_data.key_low, key, 4);
	    bcopy(&a_name_data.key_high, ((long *) key) + 1, 4);

	    /* unseal the a_name key from the master key */
	    kdb_encrypt_key(&key, &key, &master_key, 
			    master_key_schedule, DES_DECRYPT);

	    create_ciph(ciph, session_key, s_name_data.name,
			s_name_data.instance, local_realm, lifetime,
		  s_name_data.key_version, tk, kerb_time.tv_sec, &key);

	    /* clear session key */
	    bzero(session_key, sizeof(session_key));

	    bzero(key, sizeof(key));



	    /* always send a reply packet */
	    rpkt = create_auth_reply(req_name_ptr, req_inst_ptr,
		req_realm_ptr, req_time_ws, 0, a_name_data.exp_date,
		a_name_data.key_version, ciph);
	    sendto(f, rpkt->dat, rpkt->length, 0, (struct sockaddr*)client, S_AD_SZ);
	    bzero(&a_name_data, sizeof(a_name_data));
	    bzero(&s_name_data, sizeof(s_name_data));
	    break;
	}
    case AUTH_MSG_APPL_REQUEST:
	{
	    u_long  time_ws;	/* Workstation time */
	    u_long  req_life;	/* Requested liftime */
	    char   *service;	/* Service name */
	    char   *instance;	/* Service instance */
	    int     kerno;	/* Kerberos error number */
	    char    tktrlm[REALM_SZ];

	    n_appl_req++;
	    tk->length = 0;
	    k_flags = 0;	/* various kerberos flags */

	    auth->length = 4 + strlen((char*)pkt->dat + 3);
	    auth->length += (int) *(pkt->dat + auth->length) +
		(int) *(pkt->dat + auth->length + 1) + 2;

	    bcopy(pkt->dat, auth->dat, auth->length);

	    strncpy(tktrlm, (char*)(auth->dat + 3), REALM_SZ);
	    if (set_tgtkey(tktrlm)) {
		lt = klog(L_ERR_UNK,
		    "FAILED realm %s unknown. Host: %s ",
			  tktrlm, inet_ntoa(client_host));
		kerb_err_reply(client, pkt, kerno, lt);
		return;
	    }
	    kerno = krb_rd_req(auth, "ktbtgt", tktrlm, client_host.s_addr,
		ad, 0);

	    if (kerno) {
		klog(L_ERR_UNK, "FAILED krb_rd_req from %s: %s",
		     inet_ntoa(client_host), krb_err_txt[kerno]);
		kerb_err_reply(client, pkt, kerno, "krb_rd_req failed");
		return;
	    }
	    ptr = (char *) pkt->dat + auth->length;

	    bcopy(ptr, &time_ws, 4);
	    ptr += 4;

	    req_life = (unsigned char) (*ptr++);

	    service = ptr;
	    instance = ptr + strlen(service) + 1;

	    klog(L_APPL_REQ, "APPL Request %s.%s@%s on %s for %s.%s",
	     ad->pname, ad->pinst, ad->prealm, inet_ntoa(client_host),
		 service, instance, 0);

	    if (strcmp(ad->prealm, tktrlm)) {
		kerb_err_reply(client, pkt, KERB_ERR_PRINCIPAL_UNKNOWN,
		     "Can't hop realms");
		return;
	    }
	    if (!strcmp(service, "changepw")) {
		kerb_err_reply(client, pkt, KERB_ERR_PRINCIPAL_UNKNOWN,
		     "Can't authorize password changed based on TGT");
		return;
	    }
	    kerno = check_princ(service, instance, req_life,
		&s_name_data);
	    if (kerno) {
		kerb_err_reply(client, pkt, kerno, lt);
		return;
	    }
	    /* Bound requested lifetime with service and user */
	    lifetime = min(req_life,
	      krb_time_to_life(kerb_time.tv_sec,krb_life_to_time(ad->time_sec,ad->life)));
	    lifetime = min(lifetime, ((u_long) s_name_data.max_life));

	    /* unseal server's key from master key */
	    bcopy(&s_name_data.key_low, key, 4);
	    bcopy(&s_name_data.key_high, ((long *) key) + 1, 4);
	    kdb_encrypt_key(&key, &key, &master_key,
			    master_key_schedule, DES_DECRYPT);
	    /* construct and seal the ticket */

#ifdef NOENCRYPTION
	    bzero(session_key, sizeof(des_cblock));
#else
	    des_new_random_key(&session_key);
#endif

	    krb_create_ticket(tk, k_flags, ad->pname, ad->pinst,
			      ad->prealm, client_host.s_addr,
			      session_key, lifetime, kerb_time.tv_sec,
			      s_name_data.name, s_name_data.instance,
			      &key);
	    bzero(key, sizeof(key));
	    bzero(key_s, sizeof(key_s));

	    create_ciph(ciph, session_key, service, instance,
			local_realm,
			lifetime, s_name_data.key_version, tk,
			kerb_time.tv_sec, &ad->session);

	    /* clear session key */
	    bzero(session_key, sizeof(session_key));

	    bzero(ad->session, sizeof(ad->session));

	    rpkt = create_auth_reply(ad->pname, ad->pinst,
				     ad->prealm, time_ws,
				     0, 0, 0, ciph);
	    sendto(f, rpkt->dat, rpkt->length, 0, (struct sockaddr*)client, S_AD_SZ);
	    bzero(&s_name_data, sizeof(s_name_data));
	    break;
	}


#ifdef notdef_DIE
    case AUTH_MSG_DIE:
	{
	    lt = klog(L_DEATH_REQ,
	        "Host: %s User: \"%s\" \"%s\" Kerberos killed",
	        inet_ntoa(client_host), req_name_ptr, req_inst_ptr, 0);
	    exit(0);
	}
#endif /* notdef_DIE */

    default:
	{
	    lt = klog(L_KRB_PERR,
		"Unknown message type: %d from %s port %u",
		req_msg_type, inet_ntoa(client_host),
		ntohs(client->sin_port));
	    break;
	}
    }
}

/*
 * setup_disc 
 *
 * disconnect all descriptors, remove ourself from the process
 * group that spawned us. 
 */

static void
setup_disc(void)
{
    int     s;

    for (s = 0; s < 3; s++) {
	(void) close(s);
    }

    (void) open("/dev/null", 0);
    (void) dup2(0, 1);
    (void) dup2(0, 2);

    setsid();

    (void) chdir("/tmp");
    return;
}

/*
 * Make sure that database isn't stale.
 *
 * Exit if it is; we don't want to tell lies.
 */

static void check_db_age(void)
{
    long age;
    
    if (max_age != -1) {
	/* Requires existance of kerb_get_db_age() */
	gettimeofday(&kerb_time, 0);
	age = kerb_get_db_age();
	if (age == 0) {
	    klog(L_KRB_PERR, "Database currently being updated!");
	    hang();
	}
	if ((age + max_age) < kerb_time.tv_sec) {
	    klog(L_KRB_PERR, "Database out of date!");
	    hang();
	    /* NOTREACHED */
	}
    }
}

int
main(int argc, char **argv)
{
    struct sockaddr_in from;
    register int n;
    int     on = 1;
    int     child;
    struct servent *sp;
    int     fromlen;
    static KTEXT_ST pkt_st;
    KTEXT   pkt = &pkt_st;
    int     kerror;
    int c;
    extern char *optarg;
    extern int optind;

    progname = argv[0];

    while ((c = getopt(argc, argv, "snmp:a:l:r:")) != EOF) {
	switch(c) {
	case 's':
	    /*
	     * Set parameters to slave server defaults.
	     */
	    if (max_age == -1 && !nflag)
		max_age = ONE_DAY;	/* 24 hours */
	    if (pause_int == -1)
		pause_int = FIVE_MINUTES; /* 5 minutes */
	    if (lflag == 0) {
		log_file = KRBSLAVELOG;
		lflag++;
	    }
	    break;
	case 'n':
	    max_age = -1;	/* don't check max age. */
	    nflag++;
	    break;
	case 'm':
	    mflag++;		/* running manually; prompt for master key */
	    break;
	case 'p':
	    /* Set pause interval. */
	    if (!isdigit(optarg[0]))
		usage();
	    pause_int = atoi(optarg);
	    if ((pause_int < 5) ||  (pause_int > ONE_HOUR)) {
		fprintf(stderr, "pause_int must be between 5 and 3600 seconds.\n");
		usage();
	    }
	    break;
	case 'a':
	    /* Set max age. */
	    if (!isdigit(optarg[0])) 
		usage();
	    max_age = atoi(optarg);
	    if ((max_age < ONE_HOUR) || (max_age > THREE_DAYS)) {
		fprintf(stderr, "max_age must be between one hour and three days, in seconds\n");
		usage();
	    }
	    break;
	case 'l':
	    /* Set alternate log file */
	    lflag++;
	    log_file = optarg;
	    break;
	case 'r':
	    /* Set realm name */
	    rflag++;
	    strcpy(local_realm, optarg);
	    break;
	default:
	    usage();
	    break;
	}
    }

    if (optind == (argc-1)) {
	if (kerb_db_set_name(argv[optind]) != 0) {
	    fprintf(stderr, "Could not set alternate database name\n");
	    exit(1);
	}
	optind++;
    }

    if (optind != argc)
	usage();
	
    printf("Kerberos server starting\n");
    
    if ((!nflag) && (max_age != -1))
	printf("\tMaximum database age: %d seconds\n", max_age);
    if (pause_int != -1)
	printf("\tSleep for %d seconds on error\n", pause_int);
    else
	printf("\tSleep forever on error\n");
    if (mflag)
	printf("\tMaster key will be entered manually\n");
    
    printf("\tLog file is %s\n", lflag ? log_file : KRBLOG);

    if (lflag)
	kset_logfile(log_file);
    
    /* find our hostname, and use it as the instance */
    if (gethostname(k_instance, INST_SZ)) {
	fprintf(stderr, "%s: gethostname error\n", progname);
	exit(1);
    }

    if ((sp = getservbyname("kerberos", "udp")) == 0) {
	fprintf(stderr, "%s: udp/kerberos unknown service\n", progname);
	exit(1);
    }
    sina.sin_port = sp->s_port;

    if ((f = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	fprintf(stderr, "%s: Can't open socket\n", progname);
	exit(1);
    }
    if (setsockopt(f, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
	fprintf(stderr, "%s: setsockopt (SO_REUSEADDR)\n", progname);

    if (bind(f, (struct sockaddr*)&sina, sizeof(sina)) < 0) {
	fprintf(stderr, "%s: Can't bind socket\n", progname);
	exit(1);
    }
    /* do all the database and cache inits */
    if ((n = kerb_init())) {
	if (mflag) {
	    printf("Kerberos db and cache init ");
	    printf("failed = %d ...exiting\n", n);
	    exit(-1);
	} else {
	    klog(L_KRB_PERR,
	    "Kerberos db and cache init failed = %d ...exiting", n);
	    hang();
	}
    }

    /* Make sure database isn't stale */
    check_db_age();
    
    /* setup master key */
    if (kdb_get_master_key (mflag, &master_key, master_key_schedule) != 0) {
      klog (L_KRB_PERR, "kerberos: couldn't get master key.\n");
      exit (-1);
    }
    kerror = kdb_verify_master_key (&master_key, master_key_schedule, stdout);
    if (kerror < 0) {
      klog (L_KRB_PERR, "Can't verify master key.");
      bzero (master_key, sizeof (master_key));
      bzero (master_key_schedule, sizeof (master_key_schedule));
      exit (-1);
    }

    master_key_version = (u_char) kerror;

    fprintf(stdout, "\nCurrent Kerberos master key version is %d\n",
	    master_key_version);
    des_init_random_number_generator(&master_key);

    if (!rflag) {
	/* Look up our local realm */
	krb_get_lrealm(local_realm, 1);
    }
    fprintf(stdout, "Local realm: %s\n", local_realm);
    fflush(stdout);

    if (set_tgtkey(local_realm)) {
	/* Ticket granting service unknown */
	klog(L_KRB_PERR, "Ticket granting ticket service unknown");
	fprintf(stderr, "Ticket granting ticket service unknown\n");
	exit(1);
    }
    if (mflag) {
	if ((child = fork()) != 0) {
	    printf("Kerberos started, PID=%d\n", child);
	    exit(0);
	}
	setup_disc();
    }
    /* receive loop */
    for (;;) {
	fromlen = S_AD_SZ;
	n = recvfrom(f, pkt->dat, MAX_PKT_LEN, 0, (struct sockaddr*)&from, &fromlen);
	if (n > 0) {
	    pkt->length = n;
	    pkt->mbz = 0; /* force zeros to catch runaway strings */
	    /* see what is left in the input queue */
	    ioctl(f, FIONREAD, &q_bytes);
	    gettimeofday(&kerb_time, NULL);
	    q_n++;
	    max_q_n = max(max_q_n, q_n);
	    n_packets++;
	    klog(L_NET_INFO,
	 "q_byt %d, q_n %d, rd_byt %d, mx_q_b %d, mx_q_n %d, n_pkt %d",
		 q_bytes, q_n, n, max_q_bytes, max_q_n, n_packets, 0);
	    max_q_bytes = max(max_q_bytes, q_bytes);
	    if (!q_bytes)
		q_n = 0;	/* reset consecutive packets */
	    kerberos(&from, pkt);
	} else
	    klog(L_NET_ERR,
	    "%s: bad recvfrom n = %d errno = %d", progname, n, errno, 0);
    }
}
