/*	$OpenBSD: perl_extern.h,v 1.4 2001/01/29 01:58:47 niklas Exp $	*/

int perl_end(GS *);
int perl_init(SCR *);
int perl_screen_end(SCR*);
int perl_ex_perl(SCR*, CHAR_T *, size_t, recno_t, recno_t);
int perl_ex_perldo(SCR*, CHAR_T *, size_t, recno_t, recno_t);
#ifdef USE_SFIO
Sfdisc_t* sfdcnewnvi(SCR*);
#endif
