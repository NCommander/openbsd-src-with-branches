/*	$Id: kadm_funcs.c,v 1.1.1.1 1995/12/14 06:52:49 tholo Exp $	*/

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
 * Kerberos administration server-side database manipulation routines
 */

/*
kadm_funcs.c
the actual database manipulation code
*/

#include <kadm_locl.h>
#include <sys/param.h>

static int
check_access(char *pname, char *pinst, char *prealm, enum acl_types acltype)
{
    char checkname[MAX_K_NAME_SZ];
    char filename[MAXPATHLEN];

    (void) snprintf(checkname, sizeof(checkname), "%s.%s@%s", pname, pinst,
		    prealm);
    
    switch (acltype) {
    case ADDACL:
	(void) snprintf(filename, sizeof(filename), "%s%s", acldir, ADD_ACL_FILE);
	break;
    case GETACL:
	(void) snprintf(filename, sizeof(filename), "%s%s", acldir, GET_ACL_FILE);
	break;
    case MODACL:
	(void) snprintf(filename, sizeof(filename), "%s%s", acldir, MOD_ACL_FILE);
	break;
    }
    return(acl_check(filename, checkname));
}

static int
wildcard(char *str)
{
    if (!strcmp(str, WILDCARD_STR))
	return(1);
    return(0);
}

#define failadd(code) {  (void) log("FAILED addding '%s.%s' (%s)", valsin->name, valsin->instance, error_message(code)); return code; }

int
kadm_add_entry (char *rname, char *rinstance, char *rrealm, Kadm_vals *valsin, Kadm_vals *valsout)
            				/* requestors name */
                			/* requestors instance */
             				/* requestors realm */
                  
                   
{
  long numfound;		/* check how many we get written */
  int more;			/* pointer to more grabbed records */
  Principal data_i, data_o;		/* temporary principal */
  u_char flags[4];
  des_cblock newpw;
  Principal default_princ;

  if (!check_access(rname, rinstance, rrealm, ADDACL)) {
    (void) log("WARNING: '%s.%s@%s' tried to add an entry for '%s.%s'",
	       rname, rinstance, rrealm, valsin->name, valsin->instance);
    return KADM_UNAUTH;
  }
  
  /* Need to check here for "legal" name and instance */
  if (wildcard(valsin->name) || wildcard(valsin->instance)) {
      failadd(KADM_ILL_WILDCARD);
  }

  (void) log("request to add an entry for '%s.%s' from '%s.%s@%s'",
		 valsin->name, valsin->instance, rname, rinstance, rrealm);
  
  numfound = kerb_get_principal(KERB_DEFAULT_NAME, KERB_DEFAULT_INST,
				&default_princ, 1, &more);
  if (numfound == -1) {
      failadd(KADM_DB_INUSE);
  } else if (numfound != 1) {
      failadd(KADM_UK_RERROR);
  }

  kadm_vals_to_prin(valsin->fields, &data_i, valsin);
  (void) strncpy(data_i.name, valsin->name, ANAME_SZ);
  (void) strncpy(data_i.instance, valsin->instance, INST_SZ);

  if (!IS_FIELD(KADM_EXPDATE,valsin->fields))
	  data_i.exp_date = default_princ.exp_date;
  if (!IS_FIELD(KADM_ATTR,valsin->fields))
      data_i.attributes = default_princ.attributes;
  if (!IS_FIELD(KADM_MAXLIFE,valsin->fields))
      data_i.max_life = default_princ.max_life; 

  bzero((char *)&default_princ, sizeof(default_princ));

  /* convert to host order */
  data_i.key_low = ntohl(data_i.key_low);
  data_i.key_high = ntohl(data_i.key_high);


  bcopy(&data_i.key_low,newpw,4);
  bcopy(&data_i.key_high,(char *)(((long *) newpw) + 1),4);

  /* encrypt new key in master key */
  kdb_encrypt_key (&newpw, &newpw, &server_parm.master_key,
		     server_parm.master_key_schedule, DES_ENCRYPT);
  bcopy(newpw,&data_i.key_low,4);
  bcopy((char *)(((long *) newpw) + 1), &data_i.key_high,4);
  bzero((char *)newpw, sizeof(newpw));

  data_o = data_i;
  numfound = kerb_get_principal(valsin->name, valsin->instance, 
				&data_o, 1, &more);
  if (numfound == -1) {
      failadd(KADM_DB_INUSE);
  } else if (numfound) {
      failadd(KADM_INUSE);
  } else {
    data_i.key_version++;
    data_i.kdc_key_ver = server_parm.master_key_version;
    (void) strncpy(data_i.mod_name, rname, sizeof(data_i.mod_name)-1);
    (void) strncpy(data_i.mod_instance, rinstance,
		   sizeof(data_i.mod_instance)-1);

    numfound = kerb_put_principal(&data_i, 1);
    if (numfound == -1) {
	failadd(KADM_DB_INUSE);
    } else if (numfound) {
	failadd(KADM_UK_SERROR);
    } else {
      numfound = kerb_get_principal(valsin->name, valsin->instance, 
				    &data_o, 1, &more);
      if ((numfound!=1) || (more!=0)) {
	  failadd(KADM_UK_RERROR);
      }
      bzero((char *)flags, sizeof(flags));
      SET_FIELD(KADM_NAME,flags);
      SET_FIELD(KADM_INST,flags);
      SET_FIELD(KADM_EXPDATE,flags);
      SET_FIELD(KADM_ATTR,flags);
      SET_FIELD(KADM_MAXLIFE,flags);
      kadm_prin_to_vals(flags, valsout, &data_o);
      (void) log("'%s.%s' added.", valsin->name, valsin->instance);
      return KADM_DATA;		/* Set all the appropriate fields */
    }
  }
}
#undef failadd

#define failget(code) {  (void) log("FAILED retrieving '%s.%s' (%s)", valsin->name, valsin->instance, error_message(code)); return code; }

int
kadm_get_entry (char *rname, char *rinstance, char *rrealm, Kadm_vals *valsin, u_char *flags, Kadm_vals *valsout)
            				/* requestors name */
                			/* requestors instance */
             				/* requestors realm */
                  			/* what they wannt to get */
              				/* which fields we want */
                   			/* what data is there */
{
  long numfound;		/* check how many were returned */
  int more;			/* To point to more name.instances */
  Principal data_o;		/* Data object to hold Principal */

  
  if (!check_access(rname, rinstance, rrealm, GETACL)) {
    (void) log("WARNING: '%s.%s@%s' tried to get '%s.%s's entry",
	    rname, rinstance, rrealm, valsin->name, valsin->instance);
    return KADM_UNAUTH;
  }
  
  if (wildcard(valsin->name) || wildcard(valsin->instance)) {
      failget(KADM_ILL_WILDCARD);
  }

  (void) log("retrieve '%s.%s's entry for '%s.%s@%s'",
	     valsin->name, valsin->instance, rname, rinstance, rrealm);
  
  /* Look up the record in the database */
  numfound = kerb_get_principal(valsin->name, valsin->instance, 
				&data_o, 1, &more);
  if (numfound == -1) {
      failget(KADM_DB_INUSE);
  }  else if (numfound) {	/* We got the record, let's return it */
    kadm_prin_to_vals(flags, valsout, &data_o);
    (void) log("'%s.%s' retrieved.", valsin->name, valsin->instance);
    return KADM_DATA;		/* Set all the appropriate fields */
  } else {
      failget(KADM_NOENTRY);	/* Else whimper and moan */
  }
}
#undef failget

#define failmod(code) {  (void) log("FAILED modifying '%s.%s' (%s)", valsin1->name, valsin1->instance, error_message(code)); return code; }

int
kadm_mod_entry (char *rname, char *rinstance, char *rrealm, Kadm_vals *valsin1, Kadm_vals *valsin2, Kadm_vals *valsout)
            				/* requestors name */
                			/* requestors instance */
             				/* requestors realm */
                             		/* holds the parameters being
					   passed in */
                   		/* the actual record which is returned */
{
  long numfound;
  int more;
  Principal data_o, temp_key;
  u_char fields[4];
  des_cblock newpw;

  if (wildcard(valsin1->name) || wildcard(valsin1->instance)) {
      failmod(KADM_ILL_WILDCARD);
  }
  
  if (!check_access(rname, rinstance, rrealm, MODACL)) {
    (void) log("WARNING: '%s.%s@%s' tried to change '%s.%s's entry",
	       rname, rinstance, rrealm, valsin1->name, valsin1->instance);
    return KADM_UNAUTH;
  }
  
  (void) log("request to modify '%s.%s's entry from '%s.%s@%s' ",
	     valsin1->name, valsin1->instance, rname, rinstance, rrealm);
  
  numfound = kerb_get_principal(valsin1->name, valsin1->instance, 
				&data_o, 1, &more);
  if (numfound == -1) {
      failmod(KADM_DB_INUSE);
  } else if (numfound) {
      kadm_vals_to_prin(valsin2->fields, &temp_key, valsin2);
      (void) strncpy(data_o.name, valsin1->name, ANAME_SZ);
      (void) strncpy(data_o.instance, valsin1->instance, INST_SZ);
      if (IS_FIELD(KADM_EXPDATE,valsin2->fields))
	  data_o.exp_date = temp_key.exp_date;
      if (IS_FIELD(KADM_ATTR,valsin2->fields))
	  data_o.attributes = temp_key.attributes;
      if (IS_FIELD(KADM_MAXLIFE,valsin2->fields))
	  data_o.max_life = temp_key.max_life; 
      if (IS_FIELD(KADM_DESKEY,valsin2->fields)) {
	  data_o.key_version++;
	  data_o.kdc_key_ver = server_parm.master_key_version;


	  /* convert to host order */
	  temp_key.key_low = ntohl(temp_key.key_low);
	  temp_key.key_high = ntohl(temp_key.key_high);


	  bcopy(&temp_key.key_low,newpw,4);
	  bcopy(&temp_key.key_high,(char *)(((long *) newpw) + 1),4);

	  /* encrypt new key in master key */
	  kdb_encrypt_key (&newpw, &newpw, &server_parm.master_key,
			   server_parm.master_key_schedule, DES_ENCRYPT);
	  bcopy(newpw,&data_o.key_low,4);
	  bcopy((char *)(((long *) newpw) + 1), &data_o.key_high,4);
	  bzero((char *)newpw, sizeof(newpw));
      }
      bzero((char *)&temp_key, sizeof(temp_key));

      (void) strncpy(data_o.mod_name, rname, sizeof(data_o.mod_name)-1);
      (void) strncpy(data_o.mod_instance, rinstance,
		     sizeof(data_o.mod_instance)-1);
      more = kerb_put_principal(&data_o, 1);

      bzero((char *)&data_o, sizeof(data_o));

      if (more == -1) {
	  failmod(KADM_DB_INUSE);
      } else if (more) {
	  failmod(KADM_UK_SERROR);
      } else {
	  numfound = kerb_get_principal(valsin1->name, valsin1->instance, 
					&data_o, 1, &more);
	  if ((more!=0)||(numfound!=1)) {
	      failmod(KADM_UK_RERROR);
	  }
	  bzero((char *) fields, sizeof(fields));
	  SET_FIELD(KADM_NAME,fields);
	  SET_FIELD(KADM_INST,fields);
	  SET_FIELD(KADM_EXPDATE,fields);
	  SET_FIELD(KADM_ATTR,fields);
	  SET_FIELD(KADM_MAXLIFE,fields);
	  kadm_prin_to_vals(fields, valsout, &data_o);
	  (void) log("'%s.%s' modified.", valsin1->name, valsin1->instance);
	  return KADM_DATA;		/* Set all the appropriate fields */
      }
  }
  else {
      failmod(KADM_NOENTRY);
  }
}
#undef failmod

#define failchange(code) {  (void) log("FAILED changing key for '%s.%s@%s' (%s)", rname, rinstance, rrealm, error_message(code)); return code; }

int
kadm_change (char *rname, char *rinstance, char *rrealm, unsigned char *newpw)
{
  long numfound;
  int more;
  Principal data_o;
  des_cblock local_pw;

  if (strcmp(server_parm.krbrlm, rrealm)) {
      (void) log("change key request from wrong realm, '%s.%s@%s'!\n",
		 rname, rinstance, rrealm);
      return(KADM_WRONG_REALM);
  }

  if (wildcard(rname) || wildcard(rinstance)) {
      failchange(KADM_ILL_WILDCARD);
  }
  (void) log("'%s.%s@%s' wants to change its password",
	     rname, rinstance, rrealm);
  
  bcopy(newpw, local_pw, sizeof(local_pw));
  
  /* encrypt new key in master key */
  kdb_encrypt_key (&local_pw, &local_pw, &server_parm.master_key,
		     server_parm.master_key_schedule, DES_ENCRYPT);

  numfound = kerb_get_principal(rname, rinstance, 
				&data_o, 1, &more);
  if (numfound == -1) {
      failchange(KADM_DB_INUSE);
  } else if (numfound) {
    bcopy(local_pw,&data_o.key_low,4);
    bcopy((char *)(((long *) local_pw) + 1), &data_o.key_high,4);
    data_o.key_version++;
    data_o.kdc_key_ver = server_parm.master_key_version;
    (void) strncpy(data_o.mod_name, rname, sizeof(data_o.mod_name)-1);
    (void) strncpy(data_o.mod_instance, rinstance,
		   sizeof(data_o.mod_instance)-1);
    more = kerb_put_principal(&data_o, 1);
    bzero((char *) local_pw, sizeof(local_pw));
    bzero((char *) &data_o, sizeof(data_o));
    if (more == -1) {
	failchange(KADM_DB_INUSE);
    } else if (more) {
	failchange(KADM_UK_SERROR);
    } else {
	(void) log("'%s.%s@%s' password changed.", rname, rinstance, rrealm);
	return KADM_SUCCESS;
    }
  }
  else {
      failchange(KADM_NOENTRY);
  }
}
#undef failchange
