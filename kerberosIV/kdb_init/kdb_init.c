/*	$Id: kdb_init.c,v 1.1.1.1 1995/12/14 06:52:42 tholo Exp $	*/

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
 * program to initialize the database,  reports error if database file
 * already exists. 
 */

#include <adm_locl.h>
#include <sys/param.h>

enum ap_op {
    NULL_KEY,			/* setup null keys */
    MASTER_KEY,                 /* use master key as new key */
    RANDOM_KEY			/* choose a random key */
};

char   *progname;
static des_cblock master_key;
static des_key_schedule master_key_schedule;

/* use a return code to indicate success or failure.  check the return */
/* values of the routines called by this routine. */

static int
add_principal(char *name, char *instance, enum ap_op aap_op)
{
    Principal principal;
    struct tm *tm;
    des_cblock new_key;

    bzero(&principal, sizeof(principal));
    strncpy(principal.name, name, ANAME_SZ);
    strncpy(principal.instance, instance, INST_SZ);
    switch (aap_op) {
    case NULL_KEY:
	principal.key_low = 0;
	principal.key_high = 0;
	break;
    case RANDOM_KEY:
#ifdef NOENCRYPTION
	bzero(new_key, sizeof(des_cblock));
	new_key[0] = 127;
#else
	des_new_random_key(&new_key);
#endif
	kdb_encrypt_key (&new_key, &new_key, &master_key, master_key_schedule,
			 DES_ENCRYPT);
	bcopy(new_key, &principal.key_low, 4);
	bcopy(((long *) new_key) + 1, &principal.key_high, 4);
	break;
    case MASTER_KEY:
	bcopy (master_key, new_key, sizeof (des_cblock));
	kdb_encrypt_key (&new_key, &new_key, &master_key, master_key_schedule,
			 DES_ENCRYPT);
	bcopy(new_key, &principal.key_low, 4);
	bcopy(((long *) new_key) + 1, &principal.key_high, 4);
	break;
    }
    principal.exp_date = 946702799;	/* Happy new century */
    strncpy(principal.exp_date_txt, "12/31/99", DATE_SZ);
    principal.mod_date = time(0);

    tm = k_localtime(&principal.mod_date);
    principal.attributes = 0;
    principal.max_life = 255;

    principal.kdc_key_ver = 1;
    principal.key_version = 1;

    strncpy(principal.mod_name, "db_creation", ANAME_SZ);
    strncpy(principal.mod_instance, "", INST_SZ);
    principal.old = 0;

    if (kerb_db_put_principal(&principal, 1) != 1)
        return -1;		/* FAIL */
    
    /* let's play it safe */
    bzero (new_key, sizeof (des_cblock));
    bzero (&principal.key_low, 4);
    bzero (&principal.key_high, 4);
    return 0;
}

int
main(int argc, char **argv)
{
    char    admin[MAXHOSTNAMELEN];
    char    realm[REALM_SZ], defrealm[REALM_SZ];
    char   *cp, *dot;
    int code;
    char *database;
    
    progname = (cp = strrchr(*argv, '/')) ? cp + 1 : *argv;

    if (argc > 3) {
	fprintf(stderr, "Usage: %s [realm-name] [database-name]\n", argv[0]);
	exit(1);
    }
    if (argc == 3) {
	database = argv[2];
	--argc;
    } else
	database = DBM_FILE;

    /* Do this first, it'll fail if the database exists */
    if ((code = kerb_db_create(database)) != 0) {
	fprintf(stderr, "Couldn't create database: %s\n",
		strerror(code));
	exit(1);
    }
    kerb_db_set_name(database);

    if (argc == 2)
	strncpy(realm, argv[1], REALM_SZ);
    else {
	if (krb_get_lrealm(defrealm, 1) != KSUCCESS)
	    strcpy(defrealm, "NONE");
	fprintf(stderr, "Realm name [default  %s ]: ", defrealm);
	if (fgets(realm, sizeof(realm), stdin) == NULL) {
	    fprintf(stderr, "\nEOF reading realm\n");
	    exit(1);
	}
	if ((cp = strchr(realm, '\n')))
	    *cp = '\0';
	if (!*realm)			/* no realm given */
	    strcpy(realm, defrealm);
    }
    if (!k_isrealm(realm)) {
	fprintf(stderr, "%s: Bad kerberos realm name \"%s\"\n",
		progname, realm);
	exit(1);
    }
    printf("You will be prompted for the database Master Password.\n");
    printf("It is important that you NOT FORGET this password.\n");
    fflush(stdout);

    if (kdb_get_master_key (TRUE, &master_key, master_key_schedule) != 0) {
      fprintf (stderr, "Couldn't read master key.\n");
      exit (-1);
    }

    if (krb_get_admhst(admin, realm, 1) != KSUCCESS) {
      fprintf (stderr, "Couldn't get admin server.\n");
      exit (-1);
    }
    if ((dot = strchr(admin, '.')) != NULL)
	*dot = '\0';

    /* Initialize non shared random sequence */
    des_init_random_number_generator(&master_key);

    if (
	add_principal(KERB_M_NAME, KERB_M_INST, MASTER_KEY) ||
	add_principal(KERB_DEFAULT_NAME, KERB_DEFAULT_INST, NULL_KEY) ||
	add_principal("krbtgt", realm, RANDOM_KEY) ||
	add_principal("changepw", admin, RANDOM_KEY) 
	) {
	fprintf(stderr, "\n%s: couldn't initialize database.\n",
		progname);
	exit(1);
    }

    /* play it safe */
    bzero (master_key, sizeof (des_cblock));
    bzero (master_key_schedule, sizeof (des_key_schedule));
    exit(0);
}
