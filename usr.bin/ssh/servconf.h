/*

servconf.h

Author: Tatu Ylonen <ylo@cs.hut.fi>

Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
                   All rights reserved

Created: Mon Aug 21 15:35:03 1995 ylo

Definitions for server configuration data and for the functions reading it.

*/

/* RCSID("$Id: servconf.h,v 1.6 1999/05/04 11:59:10 bg Exp $"); */

#ifndef SERVCONF_H
#define SERVCONF_H

#define MAX_ALLOW_HOSTS		256 /* Max # hosts on allow list. */
#define MAX_DENY_HOSTS		256 /* Max # hosts on deny list. */

typedef struct
{
  int port;			/* Port number to listen on. */
  struct in_addr listen_addr;	/* Address on which the server listens. */
  char *host_key_file;		/* File containing host key. */
  char *random_seed_file;	/* File containing random seed. */
  int server_key_bits;		/* Size of the server key. */
  int login_grace_time;		/* Disconnect if no auth in this time (sec). */
  int key_regeneration_time;	/* Server key lifetime (seconds). */
  int permit_root_login;	/* If true, permit root login. */
  int ignore_rhosts;		/* Ignore .rhosts and .shosts. */
  int quiet_mode;		/* If true, don't log anything but fatals. */
  int fascist_logging;		/* Perform very verbose logging. */
  int print_motd;		/* If true, print /etc/motd. */
  int x11_forwarding;		/* If true, permit inet (spoofing) X11 fwd. */
  int strict_modes;		/* If true, require string home dir modes. */
  int keepalives;		/* If true, set SO_KEEPALIVE. */
  SyslogFacility log_facility;	/* Facility for system logging. */
  int rhosts_authentication;	/* If true, permit rhosts authentication. */
  int rhosts_rsa_authentication;/* If true, permit rhosts RSA authentication.*/
  int rsa_authentication;	/* If true, permit RSA authentication. */
#ifdef KRB4
  int kerberos_authentication;	/* If true, permit Kerberos authentication. */
  int kerberos_or_local_passwd;
#endif
#if defined(KRB4) || defined(AFS)
  int kerberos_ticket_cleanup;	/* If true, destroy ticket file on logout. */
#endif
#ifdef KERBEROS_TGT_PASSING
  int kerberos_tgt_passing;	/* If true, permit Kerberos tgt passing. */
#endif
#ifdef AFS
  int afs_token_passing;	/* If true, permit AFS token passing. */
#endif
  int password_authentication;  /* If true, permit password authentication. */
  int permit_empty_passwd;      /* If false, do not permit empty passwords. */
  unsigned int num_allow_hosts;
  char *allow_hosts[MAX_ALLOW_HOSTS];
  unsigned int num_deny_hosts;
  char *deny_hosts[MAX_DENY_HOSTS];
} ServerOptions;

/* Initializes the server options to special values that indicate that they
   have not yet been set. */
void initialize_server_options(ServerOptions *options);

/* Reads the server configuration file.  This only sets the values for those
   options that have the special value indicating they have not been set. */
void read_server_config(ServerOptions *options, const char *filename);

/* Sets values for those values that have not yet been set. */
void fill_default_server_options(ServerOptions *options);

#endif /* SERVCONF_H */
