#include <errno.h>
#include <stdlib.h>
#include "queue.h"

typedef struct node_s {
  struct node_s *next;
  struct node_s *prev;
  char *data;
} node_t;

typedef struct queue_s {
  int nelem;
  struct node_s *head;
  struct node_s *tail;
  int idx_var_off;
  int (*compar)(char*,char*);
} queue_t;

queue_t 
*create_queue(int (*compar)(char*,char*), int idx_var_off)
{
  queue_t *q= (queue_t*)calloc(1,sizeof(queue_t));
  q->compar = compar;
  q->idx_var_off = idx_var_off;
  return q;
}

int
add_to_queue(queue_t *q, char *data) {
  node_t *n,*e;
  
  q->nelem++;
  
  n       = (node_t*)calloc(1,sizeof(node_t));
  n->data = data;

  if (!q->head) {
    q->head = q->tail = n;
    return 1;
  }
  e = q->head;
  
  while(e && (q->compar(e->data+q->idx_var_off,n->data+q->idx_var_off))<0) {
    e = e->next;
  }
  if (e && e!=q->head) {
    n->next = e;
    n->prev = e->prev;
    e->prev->next = n;
    e->prev = n;
  } else if (e==q->head) {
    n->next = q->head;
    q->head->prev = n;
    q->head = n;
  } else if (!e){
    n->prev = q->tail;
    q->tail->next = n;
    q->tail = n;
  }
  return 1;
}

static inline void
detach_node(queue_t *q, node_t *n)
{
  q->nelem--;
  if (n == q->head && n == q->tail) {
    q->head = q->tail = 0;
  } else if (n == q->head) {
    q->head       = q->head->next;
    q->head->prev = 0;
  } else if (n == q->tail) {
    q->tail       = q->tail->prev;
    q->tail->next = 0;
  } else {
    n->prev->next = n->next;
    n->next->prev = n->prev;
  }
  free(n);
}

char*
get_matching(queue_t *q,char *value, int detach)
{
  node_t *n;
  char* found = 0;
  int r;
  n = q->head;
  while(n && (r=(q->compar(n->data+q->idx_var_off,value)))<=0) {
    if (r==0) {
      found = n->data;
      break;
    }
    n = n->next;
  }
  if (found && detach) 
    detach_node(q,n);

  return found;
}

char*
get_less_than(queue_t *q,char *value, int detach)
{
  node_t *n;
  char* found = 0;
  int r;
  n = q->head;

  while(n) {  
          if (q->compar(n->data+q->idx_var_off,value)<0) {
                  found = n->data;
                  break;
          }
          n = n->next;
  }

  if (found && detach) 
    detach_node(q,n);
  return found;
}

char*
get_greater_than(queue_t *q,char *value, int detach)
{
  node_t *n;
  char* found = 0;

  n = q->head;
  while(n) {
    if (q->compar(n->data+q->idx_var_off,value)>0) {
      found = n->data;
      break;
    }
    n = n->next;
  }
  
  if (found && detach) 
    detach_node(q,n);

  return found;
}

char*
get_item_no(queue_t *q,int n, int detach) 
{
  node_t *np;
  char *found=0;
  int i=0;
  
  if (n>=q->nelem) return (char*)0;
  np = q->head;
  while(i++<n)
    np=np->next;
  found = np->data;
  if (detach) detach_node(q,np);

  return found;
}
  
inline int
queue_len(queue_t *q)
{
  return q->nelem;
}








