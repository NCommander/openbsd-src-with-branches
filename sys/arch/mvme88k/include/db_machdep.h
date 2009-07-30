/* $OpenBSD: db_machdep.h,v 1.29 2004/04/26 14:31:11 miod Exp $ */
/* public domain */
#include <m88k/db_machdep.h>

#ifdef DDB

void m88k_db_prom_cmd(db_expr_t, int, db_expr_t, char *);

#define	EXTRA_MACHDEP_COMMANDS \
	{ "prom",	m88k_db_prom_cmd,		0,	NULL },

#endif
