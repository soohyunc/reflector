#include <sys/types.h>
#include <sys/socket.h>

#ifndef __REFLECTOR_H__
#define __REFLECTOR_H__

#define MAX_PKT_SZ 1500

typedef struct s_channel {
  int     port;      /* there is some redundancy between host_t here */
  struct queue_s *hosts;
  int     s;         /* socket */
  float   loss;      /* the loss to be induced      */
  int     min_delay; /* the minimum delay we induce */
  int     max_delay; /* and the max delay...        */
  float   dup_pr;    /* the probability of pkt duplication */
  int recv;          /* number pkts received        */
  int forw;          /* number forwarded            */
  int drop;          /* number dropped              */
  struct refl_stat_s *err_dist;
} channel_t;

typedef struct s_pkt {
    unsigned int go_time;
    unsigned int arr_time;
    channel_t *channel;
    unsigned int src_addr;
    char *data;
    short size;
} pkt_t;

#endif


