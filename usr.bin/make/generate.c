
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include "make.h"
#include "stats.h"
#include "ohash.h"
#include "cond_int.h"

#define M(x)	x, #x
char *table_var[] = {
	M(TARGET),
	M(OODATE),
	M(ALLSRC),
	M(IMPSRC),
	M(PREFIX),
	M(ARCHIVE),
	M(MEMBER),
	M(LONGTARGET),
	M(LONGOODATE),
	M(LONGALLSRC),
	M(LONGIMPSRC),
	M(LONGPREFIX),
	M(LONGARCHIVE),
	M(LONGMEMBER),
	M(FTARGET),
	M(DTARGET),
	M(FPREFIX),
	M(DPREFIX),
	M(FARCHIVE),
	M(DARCHIVE),
	M(FMEMBER),
	M(DMEMBER),
	NULL
};

char *table_cond[] = {
	M(COND_IF),
	M(COND_IFDEF),
	M(COND_IFNDEF),
	M(COND_IFMAKE),
	M(COND_IFNMAKE),
	M(COND_ELSE),
	M(COND_ELIFDEF),
	M(COND_ELIFNDEF),
	M(COND_ELIFMAKE),
	M(COND_ELIFNMAKE),
	M(COND_ENDIF),
	M(COND_FOR),
	M(COND_INCLUDE),
	M(COND_UNDEF),
	NULL
};


char **table[] = {
	table_var,
	table_cond
};

int
main(int argc, char *argv[])
{
	u_int32_t i;
	u_int32_t v;
	u_int32_t h;
	u_int32_t slots;
	const char *e;
	char **occupied;
	char **t;
	int tn;

#ifdef HAS_STATS
	Init_Stats();
#endif
	if (argc != 3)
		exit(1);

	tn = atoi(argv[1]);
	if (!tn)
		exit(1);
	t = table[tn-1];
	slots = atoi(argv[2]);
	if (slots) {
		occupied = malloc(sizeof(char *) * slots);
		if (!occupied)
			exit(1);
		for (i = 0; i < slots; i++)
			occupied[i] = NULL;
	} else
		occupied = NULL;

	printf("/* File created by generate %d %d, do not edit */\n", 
	    tn, slots);
	for (i = 0; t[i] != NULL; i++) {
		e = NULL;
		v = ohash_interval(t[i], &e);
		if (slots) {
			h = v % slots;
			if (occupied[h]) {
				fprintf(stderr, 
				    "Collision: %s / %s (%d)\n", occupied[h],
				    t[i], h);
				exit(1);
			}
			occupied[h] = t[i];
		}
		i++;
		printf("#define K_%s %u\n", t[i], v);
	}
	if (slots)
		printf("#define MAGICSLOTS%d %u\n", tn, slots);
	exit(0);
}
