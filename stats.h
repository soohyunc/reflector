#ifndef __REFL_STATS_H_
#define __REFL_STATS_H_

struct refl_stat_s;

struct refl_stat_s *
stats_create();

void
stats_add(struct refl_stat_s *, int err);

void 
stats_dump(struct refl_stat_s *);

void 
stats_destroy(struct refl_stat_s*);

#endif /* __REFL_STATS_H_ */
