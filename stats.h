#ifndef __REFL_STATS_H_
#define __REFL_STATS_H_

struct refl_stat_s;

struct refl_stat_s *
new_refl_stat();

void
add_item_to_refl_stat(struct refl_stat_s *, int err);

void 
dmp_refl_stat(struct refl_stat_s *);

void 
free_refl_stat(struct refl_stat_s*);

#endif /* __REFL_STATS_H_ */
