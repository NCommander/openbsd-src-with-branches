/* $OpenBSD: util.h,v 1.4 2003/07/18 02:00:09 deraadt Exp $ */

/* and for those machine that can't handle a variable argument list */

EXT char	serrbuf[BUFSIZ];/* buffer for stderr */

char		*fetchname(char *, int, int);
int		move_file(char *, char *);
void		copy_file(char *, char *);
void		say(char *, ...);
void		fatal(char *, ...);
void		pfatal(char *, ...);
void		ask(char *, ...);
char		*savestr(char *);
void		set_signals(int);
void		ignore_signals(void);
void		makedirs(char *, bool);
void		version(void);
