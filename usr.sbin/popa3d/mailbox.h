/* $OpenBSD: mailbox.h,v 1.2 2001/08/13 20:19:33 camield Exp $ */

/*
 * Mailbox access.
 */

#ifndef _POP_MAILBOX_H
#define _POP_MAILBOX_H

/*
 * Opens the mailbox, filling in the message database. Returns a non-zero
 * value on error.
 */
extern int mailbox_open(char *spool, char *mailbox);

/*
 * Sends (first lines of) a message to the POP client. Returns a non-zero
 * value on error; the POP session then has to crash.
 */
extern int mailbox_get(struct db_message *msg, int lines);

/*
 * Rewrites the mailbox according to flags in the database.
 */
extern int mailbox_update(void);

/*
 * Closes the mailbox file.
 */
extern int mailbox_close(void);

#endif
