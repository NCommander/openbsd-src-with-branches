/*	$OpenBSD: client.h,v 1.2 1999/02/01 06:53:55 d Exp $	*/

/* hunt.c */
extern FLAG	Am_monitor;		/* -m flag */
extern FLAG	no_beep;		/* -b flag */
extern char *	Send_message;		/* -w message */
extern int	Socket;			/* connection to server */
extern char     map_key[256];		/* HUNT envvar */

void	bad_con(void);
void	bad_ver(void);
void	intr(int);

/* connect.c */
void	do_connect(char *, u_int8_t, u_int32_t);

/* playit.c */
void	playit(void);
void	do_message(void);
int	quit(int);

/* otto.c */
extern int	Otto_mode;
int	otto(int, int, char, char *, size_t);
int	otto_quit(int);
