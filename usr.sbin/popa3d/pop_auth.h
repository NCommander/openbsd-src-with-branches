/* $OpenBSD: pop_auth.h,v 1.1 2001/08/19 13:05:57 deraadt Exp $ */

/*
 * AUTHORIZATION state handling.
 */

#ifndef _POP_AUTH_H
#define _POP_AUTH_H

/*
 * Possible authentication results.
 */
#define AUTH_OK				0
#define AUTH_NONE			1
#define AUTH_FAILED			2

/*
 * Handles the AUTHORIZATION state commands, and writes authentication
 * data into the channel.
 */
extern int do_pop_auth(int channel);

/*
 * Logs an authentication attempt for user, use NULL for non-existent.
 */
extern void log_pop_auth(int result, char *user);

#endif
