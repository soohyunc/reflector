/* ------------------------------------------------------------------------- */
/* (C) 1996-2000 Orion Hodson.                                               */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_BINS 32

typedef struct refl_stat_s {
    unsigned int bin[MAX_BINS];
} refl_stat_t;

/* bin width of 5ms */
#define DELAYTOBIN(d) (d/5)

#define min(a,b)        (a < b ? a : b)
#define max(a,b)        (a > b ? a : b)

refl_stat_t *
stats_create()
{
    refl_stat_t *r = (refl_stat_t*) malloc (sizeof(refl_stat_t));
    memset(r->bin, 0, sizeof(unsigned int) * MAX_BINS);
    return r;
}

void
stats_add(refl_stat_t *r, int err)
{
    int idx = min(DELAYTOBIN(err), MAX_BINS-1);
    r->bin[idx]++;
}

void
stats_dump(refl_stat_t *r)
{
    int i;
    for(i = 0; i < MAX_BINS; i++) 
	printf("%02d\t%d\n", 5*i, r->bin[i]);
    return;
}

void
stats_destroy(refl_stat_t *r)
{
    free(r);
}
