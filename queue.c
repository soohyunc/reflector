#include <errno.h>
#include <stdlib.h>

#include "queue.h"

typedef struct node_s {
    struct node_s *next;
    struct node_s *prev;
    char *data;
} node_t;

typedef struct queue_s {
    struct node_s sentinel;    
    int (*compar)(char*,char*); /* comparison function */
    int coff;                   /* byte offset of data passed to comp. fn */
    int nelem;
} queue_t;

struct queue_s *
queue_create(int (*compar)(char*,char*), int coff)
{
    queue_t *q= (queue_t*)calloc(1, sizeof(queue_t));
    q->sentinel.next = q->sentinel.prev = &q->sentinel;
    q->compar        = compar;
    q->coff   = coff;
    q->nelem         = 0;
    return q;
}

int
queue_insert(queue_t *q, char *data) {
    node_t *n, *e, *s;
    
    n = (node_t*)calloc(1, sizeof(node_t));
    if (!n) {
	return 0;
    }
    n->data = data;

    s = &q->sentinel; 
    e = s->next;
    
    while(e != s) {
	if (q->compar(e->data + q->coff, n->data + q->coff) < 0)
	    break;
	e = e->next;
    }

    n->next = e;
    n->prev = e->prev;
    e->prev->next = n;
    e->prev = n;

    q->nelem++;

    return 1;
}

static inline void
detach_node(queue_t *q, node_t *n)
{
    n->next->prev = n->prev;
    n->prev->next = n->next;
    q->nelem--;
    free(n);
}

static char*
queue_find(queue_t *q,char *value, int detach, int cmp_val)
{
    node_t *e, *s;
    char   *data = 0;

    s = &q->sentinel;
    e = s->next;
    while (e != s) {
	if (q->compar(e->data + q->coff, value) == cmp_val) {
	    data = e->data;
	    if (detach)       
		detach_node(q, e);
	    break;
	}
	e = e->next;
    }
  
    return data;
}

char *
queue_get_eq(queue_t *q, char *value, int detach) {
    return queue_find(q, value, detach, Q_CMP_EQ);
}

char *
queue_get_lt(queue_t *q, char *value, int detach) {
    return queue_find(q, value, detach, Q_CMP_LT);
}

char *
queue_get_gt(queue_t *q, char *value, int detach) {
    return queue_find(q, value, detach, Q_CMP_GT);
}

char*
queue_get(queue_t *q, int n, int detach) 
{
    node_t *e, *s;
    char   *data = 0;

    s = &q->sentinel;
    e = s->next;

    while (e != s) {
	n--;
	if (n < 0) {
	    data = e->data;
	    if (detach)
		detach_node(q, e);
	    break;
	}
    }
  
    return data;
}
  
inline int
queue_length(queue_t *q)
{
    return q->nelem;
}








