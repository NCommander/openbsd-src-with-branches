/* $OpenBSD: virtual.h,v 1.2 2001/08/13 20:19:33 camield Exp $ */

/*
 * Virtual domain support.
 */

#ifndef _POP_VIRTUAL_H
#define _POP_VIRTUAL_H

#include <pwd.h>
#include <sys/types.h>

/*
 * These are set by the authentication routine, below.
 */
extern char *virtual_domain;
extern char *virtual_spool;

/*
 * Initializes the virtual domain support at startup. Note that this will
 * only be called once in standalone mode, so don't expect an open socket
 * here. Returns a non-zero value on error.
 */
extern int virtual_startup(void);

/*
 * Tries to authenticate a username/password pair for the virtual domain
 * indicated either by the connected IP address (the socket is available
 * on fd 0), or as a part of the username. If the virtual domain is known,
 * virtual_domain and virtual_spool are set appropriately. If the username
 * is known as well, mailbox is set to the username. Returns the template
 * user to run as if the authentication is successful, or NULL otherwise.
 */
extern struct passwd *virtual_userpass(char *user, char *pass, char **mailbox);

#endif
