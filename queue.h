#ifndef __QUEUE_H__
#define __QUEUE_H__

#define Q_KEEP   0
#define Q_DETACH 1

struct node_s;
struct queue_s;

struct queue_s* create_queue(int(*compar)(char*,char*), int idx_var_off);
int add_to_queue(struct queue_s*,char *data);
char* get_matching(struct queue_s*,char *value, int detach);
char* get_less_than(struct queue_s*,char *value, int detach);
char* get_greater_than(struct queue_s*,char *value, int detach);
char* get_item_no(struct queue_s*,int n, int detach);
int queue_len(struct queue_s *);

#endif /* __QUEUE_H__ */
