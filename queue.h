#ifndef __QUEUE_H__
#define __QUEUE_H__

#define Q_KEEP   0
#define Q_DETACH 1

#define Q_CMP_GT  1
#define Q_CMP_LT -1
#define Q_CMP_EQ  0

struct node_s;
struct queue_s;

struct queue_s* queue_create(int(*compar)(char*,char*), int idx_var_off);
int   queue_insert(struct queue_s*,char *data);
char* queue_get_eq(struct queue_s*,char *value, int detach);
char* queue_get_lt(struct queue_s*,char *value, int detach);
char* queue_get_gt(struct queue_s*,char *value, int detach);
char* queue_get(struct queue_s*,int n, int detach);
int   queue_length(struct queue_s *);

#endif /* __QUEUE_H__ */
