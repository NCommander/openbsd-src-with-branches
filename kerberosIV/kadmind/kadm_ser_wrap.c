/*	$Id: kadm_ser_wrap.c,v 1.1.1.1 1995/12/14 06:52:49 tholo Exp $	*/

/*-
 * Copyright (C) 1989 by the Massachusetts Institute of Technology
 *
 * Export of this software from the United States of America is assumed
 * to require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/*
 * Kerberos administration server-side support functions
 */

/* 
kadm_ser_wrap.c
unwraps wrapped packets and calls the appropriate server subroutine
*/

#include <kadm_locl.h>
#include <sys/param.h>

/* GLOBAL */
Kadm_Server server_parm;

/* 
kadm_ser_init
set up the server_parm structure
*/
int
kadm_ser_init(int inter, char *realm)
          			/* interactive or from file */
             
{
  struct servent *sep;
  struct hostent *hp;
  char *dot, admin[MAXHOSTNAMELEN], hostname[MAXHOSTNAMELEN];

  (void) init_kadm_err_tbl();
  (void) init_krb_err_tbl();
  if (gethostname(hostname, sizeof(hostname)))
      return KADM_NO_HOSTNAME;

  (void) strcpy(server_parm.sname, PWSERV_NAME);
  if (krb_get_admhst(admin, realm, 1) != KSUCCESS)
      return KADM_NO_MAST;
  if ((dot = strchr(server_parm.sinst, '.')) != NULL)
      *dot = '\0';
  (void) strcpy(server_parm.sinst, admin);
  (void) strcpy(server_parm.krbrlm, realm);

  server_parm.admin_fd = -1;
				/* setting up the addrs */
  if ((sep = getservbyname(KADM_SNAME, "tcp")) == NULL)
    return KADM_NO_SERV;
  bzero((char *)&server_parm.admin_addr,sizeof(server_parm.admin_addr));
  server_parm.admin_addr.sin_family = AF_INET;
  if ((hp = gethostbyname(hostname)) == NULL)
      return KADM_NO_HOSTNAME;
  bcopy(hp->h_addr, (char *) &server_parm.admin_addr.sin_addr.s_addr,
	hp->h_length);
  server_parm.admin_addr.sin_port = sep->s_port;
				/* setting up the database */
  if (kdb_get_master_key((inter==1), &server_parm.master_key,
			 server_parm.master_key_schedule) != 0)
    return KADM_NO_MAST;
  if ((server_parm.master_key_version =
       kdb_verify_master_key(&server_parm.master_key,
			     server_parm.master_key_schedule,stderr))<0)
      return KADM_NO_VERI;
  return KADM_SUCCESS;
}

static void errpkt(u_char **dat, int *dat_len, int code)
{
    u_int32_t retcode;
    char *pdat;

    free((char *)*dat);			/* free up req */
    *dat_len = KADM_VERSIZE + sizeof(u_int32_t);
    *dat = (u_char *) malloc((unsigned)*dat_len);
    pdat = (char *) *dat;
    retcode = htonl((u_int32_t) code);
    (void) strncpy(pdat, KADM_ULOSE, KADM_VERSIZE);
    bcopy((char *)&retcode, &pdat[KADM_VERSIZE], sizeof(u_int32_t));
    return;
}

/*
kadm_ser_in
unwrap the data stored in dat, process, and return it.
*/
int
kadm_ser_in(u_char **dat, int *dat_len)
{
    u_char *in_st;			/* pointer into the sent packet */
    int in_len,retc;			/* where in packet we are, for
					   returns */
    u_int32_t r_len;			/* length of the actual packet */
    KTEXT_ST authent;			/* the authenticator */
    AUTH_DAT ad;			/* who is this, klink */
    u_int32_t ncksum;			/* checksum of encrypted data */
    des_key_schedule sess_sched;	/* our schedule */
    MSG_DAT msg_st;
    u_char *retdat, *tmpdat;
    int retval, retlen;

    if (strncmp(KADM_VERSTR, (char *)*dat, KADM_VERSIZE)) {
	errpkt(dat, dat_len, KADM_BAD_VER);
	return KADM_BAD_VER;
    }
    in_len = KADM_VERSIZE;
    /* get the length */
    if ((retc = stv_long(*dat, &r_len, in_len, *dat_len)) < 0)
	return KADM_LENGTH_ERROR;
    in_len += retc;
    authent.length = *dat_len - r_len - KADM_VERSIZE - sizeof(u_int32_t);
    bcopy((char *)(*dat) + in_len, (char *)authent.dat, authent.length);
    authent.mbz = 0;
    /* service key should be set before here */
    if ((retc = krb_rd_req(&authent, server_parm.sname, server_parm.sinst,
			  server_parm.recv_addr.sin_addr.s_addr, &ad, (char *)0)))
    {
	errpkt(dat, dat_len,retc + krb_err_base);
	return retc + krb_err_base;
    }

#define clr_cli_secrets() {bzero((char *)sess_sched, sizeof(sess_sched)); bzero((char *)ad.session, sizeof(ad.session));}

    in_st = *dat + *dat_len - r_len;
#ifdef NOENCRYPTION
    ncksum = 0;
#else
    ncksum = des_quad_cksum((des_cblock *)in_st, (des_cblock *)0, (long) r_len, 0, &ad.session);
#endif
    if (ncksum!=ad.checksum) {		/* yow, are we correct yet */
	clr_cli_secrets();
	errpkt(dat, dat_len,KADM_BAD_CHK);
	return KADM_BAD_CHK;
    }
#ifdef NOENCRYPTION
    bzero(sess_sched, sizeof(sess_sched));
#else
    des_key_sched(&ad.session, sess_sched);
#endif
    if ((retc = (int) krb_rd_priv(in_st, r_len, sess_sched, &ad.session, 
				 &server_parm.recv_addr,
				 &server_parm.admin_addr, &msg_st))) {
	clr_cli_secrets();
	errpkt(dat, dat_len,retc + krb_err_base);
	return retc + krb_err_base;
    }
    switch (msg_st.app_data[0]) {
    case CHANGE_PW:
	retval = kadm_ser_cpw(msg_st.app_data+1,(int) msg_st.app_length,&ad,
			      &retdat, &retlen);
	break;
    case ADD_ENT:
	retval = kadm_ser_add(msg_st.app_data+1,(int) msg_st.app_length,&ad,
			      &retdat, &retlen);
	break;
    case GET_ENT:
	retval = kadm_ser_get(msg_st.app_data+1,(int) msg_st.app_length,&ad,
			      &retdat, &retlen);
	break;
    case MOD_ENT:
	retval = kadm_ser_mod(msg_st.app_data+1,(int) msg_st.app_length,&ad,
			      &retdat, &retlen);
	break;
    default:
	clr_cli_secrets();
	errpkt(dat, dat_len, KADM_NO_OPCODE);
	return KADM_NO_OPCODE;
    }
    /* Now seal the response back into a priv msg */
    free((char *)*dat);
    tmpdat = (u_char *) malloc((unsigned)(retlen + KADM_VERSIZE +
					  sizeof(u_int32_t)));
    (void) strncpy((char *)tmpdat, KADM_VERSTR, KADM_VERSIZE);
    retval = htonl((u_int32_t)retval);
    bcopy((char *)&retval, (char *)tmpdat + KADM_VERSIZE, sizeof(u_int32_t));
    if (retlen) {
	bcopy((char *)retdat, (char *)tmpdat + KADM_VERSIZE + sizeof(u_int32_t),
	      retlen);
	free((char *)retdat);
    }
    /* slop for mk_priv stuff */
    *dat = (u_char *) malloc((unsigned) (retlen + KADM_VERSIZE +
					 sizeof(u_int32_t) + 200));
    if ((*dat_len = krb_mk_priv(tmpdat, *dat,
				(u_int32_t) (retlen + KADM_VERSIZE +
					  sizeof(u_int32_t)),
				sess_sched,
				&ad.session, &server_parm.admin_addr,
				&server_parm.recv_addr)) < 0) {
	clr_cli_secrets();
	errpkt(dat, dat_len, KADM_NO_ENCRYPT);
	return KADM_NO_ENCRYPT;
    }
    clr_cli_secrets();
    return KADM_SUCCESS;
}
