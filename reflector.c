#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/filio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include "queue.h"
#include "stats.h"
#include "reflector.h"
#include "ui-controller.h"
#include "stats.h"

#define FORWARDER 0
#define REFLECTOR 1

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

static mode = REFLECTOR;
static ui_update_needed = FALSE;

void usage()
{
    char *ustr = 
	"reflector [-f] [-n] [-c <port>] [-k <known host1>,<known host2>,..]" \
	"port1 port2 ...\nwhere:\n\t -f causes packets to be forwarded " \
	"rather than reflected.\n\t -n means no user interface.\n\t " \
	"-c <port> accept control commands on the given port.\n";
    printf(ustr);
    exit(0);
}

unsigned int 
get_ms()
{
    static struct timeval base;
    struct timeval now;
    unsigned int t;
  
    if (base.tv_sec==0) {
	gettimeofday(&base,NULL);
	return 0;
    }
  
    gettimeofday(&now,NULL);
    t = (now.tv_sec-base.tv_sec)*1000 +
	(now.tv_usec-base.tv_usec)/1000;
  
    return t;
}

void 
sock_printf(int sock_fd, char *fmt, ...) 
{
    char buf[255];
    unsigned short blen;
    va_list ap;
        
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
        
    blen = strlen(buf);
    assert(blen<255);
    if (write(sock_fd, buf, blen) != blen) {
	fprintf(stderr,"lengths differ!\n");
    }

}

unsigned int
init_socket(int *pfd, unsigned short port, unsigned short proto)
{
    int lfd;
    struct sockaddr_in sinme;
    assert(pfd != NULL);

    switch (proto) {
    case IPPROTO_UDP:
	lfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	break;
    case IPPROTO_TCP:
	lfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	break;
    default:
	fprintf(stderr, "Protocol not recognized.\n");
	return FALSE;
    }

    if (lfd == -1) {
	perror("socket");
	return FALSE;
    }

    if ((port < 1024) &&
	(getuid() != 0)) {
	fprintf(stderr,"ports must be above 1024 for non-system use.\n");
	return FALSE;
    }

    memset(&sinme,0,sizeof(sinme));
    sinme.sin_family      = PF_INET;
    sinme.sin_addr.s_addr = htonl(INADDR_ANY);
    sinme.sin_port        = htons(port);
        
    if (bind(lfd, (struct sockaddr*)&sinme, sizeof(sinme))<0){
	perror("bind");
	close(lfd);
	return FALSE;
    }

    *pfd = lfd;
    return TRUE;
}

void 
init_control_sock(int *cfd, short cport) 
{
    if (init_socket(cfd, cport, IPPROTO_TCP) == FALSE) {
	fprintf(stderr, "init_control_sock failed.\n");
	exit(-1);
    }

    if (fcntl(*cfd, F_SETFL, O_NONBLOCK) == -1) {
	fprintf(stderr, "init_control_sock: non_block failed.\n");
    }

    if (listen(*cfd, 5) == -1) {
	fprintf(stderr, "listen failed.\n");
    }
        
}

/* recognized commands:
 *             helo
 *             port <n> set <loss/min_delay/max_delay> <value>
 *             port <n> get <loss/min_delay/max_delay>
 */
char *tokens[] = {
    "helo",
#define ID_HELO 0
    "port",
#define ID_PORT 1
    "set",
#define ID_SET  2
    "get",
#define ID_GET  3
    "loss",
#define ID_LOSS 4
    "min_delay",
#define ID_MIN_DELAY 5
    "max_delay",
#define ID_MAX_DELAY 6
    "dup",
#define ID_DUP  7
};
#define NTOKENS (sizeof(tokens)/sizeof(char*))

int 
token2id(char *token, int *num)
{
    int i=0;

    if (token != NULL) {
	while(i < NTOKENS) {
	    if (0 == strcmp(tokens[i],token)) {
		*num = i;
		return TRUE;
	    }
	    i++;
	}
    }
    return FALSE;
}


#define CMD_BUF_SIZE 80
void
process_cmd(int sock_fd, struct queue_s* channels)
{
    char buf[CMD_BUF_SIZE], *lbl, *sport, *cmd, *param, *value;
    channel_t *chan;
    int  nbytes, i, id, cmd_id, param_id;
    unsigned short tgt_port;
    float nloss, ndup;
    int   ndelay;

    ioctl(sock_fd, FIONREAD, &nbytes);
    if (nbytes > CMD_BUF_SIZE) {
	fprintf(stderr, "Potential buffer overflow. (%d/%d bytes)\n",
		nbytes, CMD_BUF_SIZE);
	exit(-1);
    }
       
    /* could have been a job for lex, or we could have chosen a simpler format ;-) */
    while (nbytes > 0) {
	nbytes -= read(sock_fd, buf, CMD_BUF_SIZE);
	i = 0;
	while(buf[i] != '\0' && i < CMD_BUF_SIZE) {
	    buf[i] = tolower(buf[i]);
	    if (buf[i] < ' ') {
		buf[i] = ' ';
		break;
	    }
	    i++;
	}

	lbl   = strtok(buf, " ");
	sport = strtok(NULL, " ");
	cmd   = strtok(NULL, " ");
	param = strtok(NULL, " ");
	value = strtok(NULL, " ");

	if (token2id(lbl,&id) == FALSE) {
	    sock_printf(sock_fd,"reflector token not recognized.\n");
	    continue;
	}
	switch(id) {
	case ID_HELO:
	    sock_printf(sock_fd, "helo\n");
	    break;
	case ID_PORT:
	    if (sport == NULL ||
		cmd   == NULL ||
		param == NULL) {
                                /* don't let command errors hurt reflector */

		sock_printf(sock_fd,
			    "reflector: bad command.\n");
	    }

	    chan = NULL;
	    tgt_port = atoi(sport);
	    for(i = queue_length(channels)-1; i >= 0; i--) {
		chan = (channel_t*) queue_get(channels, i, Q_KEEP);
		if (chan->port == tgt_port) break;
	    }

	    if (chan == NULL || i == -1) {
		sock_printf(sock_fd, 
			    "reflector port not recognized.\n");
		continue;
	    }
                        
	    if (token2id(cmd, &cmd_id) == FALSE) {
		sock_printf(sock_fd,
			    "reflector token not recognized.\n");
		break;
	    }
                        
	    if (token2id(param, &param_id) == FALSE) {
		sock_printf(sock_fd,
			    "reflector parameter not recognized.\n");
		break;
	    }
                        
	    switch(cmd_id) { 
	    case ID_GET:
		switch(param_id) {
		case ID_LOSS:
		    sock_printf(sock_fd, "%.2f\n", chan->loss * 100.0f);
		    break;
		case ID_MIN_DELAY:
		    sock_printf(sock_fd, "%d\n", chan->min_delay);
		    break;
		case ID_MAX_DELAY:
		    sock_printf(sock_fd, "%d\n", chan->max_delay);
		    break;
		case ID_DUP:
		    sock_printf(sock_fd, "%.2f\n", chan->dup_pr * 100.f);
		    break;
		}  /* param_id */
		break;
	    case ID_SET:
		if (value == NULL) {
		    sock_printf(sock_fd, 
				"reflector: no value supplied.\n");
		    break;
		}
		ui_update_needed = TRUE;
		switch(param_id) {
		case ID_LOSS:
		    nloss = strtod(value,NULL);
		    if (nloss>=0.0 && nloss <= 100.0f) {
			chan->loss = nloss / 100.0f;
			sock_printf(sock_fd,"ok\n");
			break;
		    }
		    sock_printf(sock_fd, 
				"reflector: invalid loss value\n");
		    break;
		case ID_MIN_DELAY:
		    ndelay = strtod(value, NULL);
		    if (ndelay > 0 && ndelay < chan->max_delay) {
			chan->min_delay = ndelay;
			sock_printf(sock_fd,"ok\n");
			break;
		    }
		    sock_printf(sock_fd, 
				"reflector: invalid min delay\n");
		    break;
		case ID_MAX_DELAY:
		    ndelay = strtod(value, NULL);
		    if (ndelay > 0 && ndelay > chan->min_delay) {
			chan->max_delay = ndelay;
			sock_printf(sock_fd,"ok\n");
			break;
		    }
		    sock_printf(sock_fd, 
				"reflector: invalid max delay\n");
		    break;
		case ID_DUP:
		    ndup = strtod(value,NULL);
		    if (ndup>=0.0 && ndup <= 100.0f) {
			chan->dup_pr = ndup / 100.0f;
			sock_printf(sock_fd,"ok\n");
			break;
		    }
		    sock_printf(sock_fd, 
				"reflector: invalid dup value\n");
		    break;

		}  /* param_id */
	    } /* cmd_id */
	} /* id */
    } /* while loop */
}


void
process_control_cmds(int control_fd,
                     struct queue_s* active_socks,
                     struct queue_s* channels)
{
    struct sockaddr incoming;
    int  len, s, i, nconn, ready;
    int *new_sock;
    char buf[80];
    fd_set strm_set;
    struct timeval notime;

    /* process incoming connections */
    len = sizeof(struct sockaddr);
    while( (s = accept(control_fd, &incoming, &len)) != -1) {
	new_sock      = malloc(sizeof(int));
	*new_sock     = s;
	queue_insert(active_socks, (char*)new_sock);
	printf("New connection accepted.\n"); 
    }

    if (errno != EWOULDBLOCK && errno != EAGAIN) {
	perror("accept");
	fprintf(stderr,"errno %d\n", errno);
	exit(-1);
    } else {
	errno = 0;
    }

    /* poll connections */
    memset(&notime,0,sizeof(struct timeval));
    nconn = queue_length(active_socks);
    FD_ZERO(&strm_set);
    for(i = 0; i < nconn; i++) {
	s = *((unsigned int*)queue_get(active_socks, i, Q_KEEP));
	FD_SET(s, &strm_set);
    }

    ready = select(16, 
		   &strm_set, 
		   (fd_set*)NULL, 
		   (fd_set*)NULL, 
                   &notime);

    if (ready > 0) {
	/* messages await */
	for(i = 0; i < nconn && ready > 0; i++) {
	    s = *((unsigned int*)queue_get(active_socks, i, Q_KEEP));
	    if (FD_ISSET(s, &strm_set)) {
		process_cmd(s, channels);
		ready--;
	    }
	}
    } else if (ready < 0) {
	perror("select");
	fprintf(stderr,"errno %d\n", errno);
	exit(-1);
    }
}

void
init_reflect_sockets(struct queue_s *channels) 
{
    channel_t *cp;
    int i,n;
    n = queue_length(channels);
    for(i=0;i<n;i++){
	cp = (channel_t*)queue_get(channels,i,Q_KEEP);
	if (init_socket(&cp->s, cp->port, IPPROTO_UDP) == FALSE) {
	    fprintf(stderr, "init_control_sock failed.\n");
	    exit(-1);
	}
    }
}

int
add_multicast(int fd, unsigned int addr)
{
    char loop       = 0, ttl = 16; /* no loopback */
    struct ip_mreq imr;
        
    imr.imr_multiaddr.s_addr = addr;
    imr.imr_interface.s_addr = INADDR_ANY;
        
    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &imr, sizeof(struct ip_mreq)) != 0) {
	perror("setsockopt IP_ADD_MEMBERSHIP");
	return FALSE;
    }

    if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) != 0) {
	perror("setsockopt IP_MULTICAST_LOOP");
	return FALSE;
    }


    if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, (char *) &ttl, sizeof(ttl)) != 0) {
	perror("setsockopt IP_MULTICAST_TTL");
	return FALSE;
    }

    return TRUE;
}

void
add_hosts(struct queue_s *channels, char *hosts)
{
    channel_t *cp;
    char *host;
    int i,n;
    unsigned int *addr;

    struct hostent *he;

    host = strtok(hosts, ",");
    n = queue_length(channels);

    while(host) {
	he = gethostbyname(host);
	if (he && he->h_length) {
	    for(i = 0; i<n; i++) {
		cp = (channel_t*)queue_get(channels,i,Q_KEEP);
		addr = (unsigned int*)malloc(sizeof(unsigned int));
		memcpy(addr, he->h_addr_list[0], 4);
		*addr = ntohl(*addr);
		if (IN_MULTICAST(*addr)) {
		    add_multicast(cp->s, *addr);
		}

		queue_insert(cp->hosts, (char*)addr);
	    }
	    printf("Added host %s\n", host);
	}
	host = strtok(NULL, ",");
    }
}

void
add_if_unknown(channel_t *cp, struct sockaddr_in *from)
{
    unsigned int *x;
    unsigned int  a;

    assert(cp->hosts);

    a = ntohl(from->sin_addr.s_addr);
    x = (unsigned int*)queue_get_eq(cp->hosts,(char*)&a, Q_KEEP);

    if (x) return;
    x = (unsigned int*)malloc(sizeof(unsigned int));
    if (x == NULL) return;
    *x = a;
    if (queue_insert(cp->hosts,(char*)x) == 0) {
	free(x);
    }
    return;
}

void
queue_pkt(channel_t *cp, struct queue_s* pkts) {
    struct sockaddr_in from;
    int         fromlen;
    pkt_t      *p;
    double      dup_pr;
    
    cp->recv ++;
    
    p           = (pkt_t *)calloc(1,sizeof(pkt_t));
    if (p == NULL) return;
    
    p->data     = (char*)malloc(MAX_PKT_SZ);
    if (p->data == NULL) {
	free(p);
	return;
    }

    p->channel  = cp;
    p->arr_time = get_ms();
    p->go_time  = p->arr_time + cp->min_delay + 
	(cp->max_delay-cp->min_delay)*drand48();
    fromlen     = sizeof(from);
    p->size     = recvfrom(cp->s, 
			   p->data, 
			   MAX_PKT_SZ, 
			   0,
			   (struct sockaddr*)&from, 
			   &fromlen);
    if (p->size <= 0) {
	perror("recvfrom");
	free(p->data);
	free(p);
	return;
    }
    p->src_addr = ntohl(from.sin_addr.s_addr);

    /* check if we've heard from this host before and add it list if not*/
    add_if_unknown(cp, &from);
    if (queue_insert(pkts,(char*)p) == 0) {
	free(p->data);
	free(p);
	return;
    }
    
    /* duplicate if necessary (ugly hack) */
    dup_pr = drand48();
    if (cp->dup_pr > dup_pr) {
	pkt_t *o    =  (pkt_t *)calloc(1,sizeof(pkt_t));

	if (o == NULL) return;
	memcpy(o, p, sizeof(pkt_t));

	o->data     =  (char*)malloc(MAX_PKT_SZ);
	if (o->data == NULL) {
	    free(o);
	    return;
	}
	memcpy(o->data, p->data, p->size);

	o->go_time  = p->arr_time + cp->min_delay + 
	    (cp->max_delay-cp->min_delay)*drand48();

	if (queue_insert(pkts,(char*)o) == 0) {
	    free(o->data);
	    free(o);
	}
    }
    return;
}

void
read_pkts(struct queue_s* channels,  struct queue_s *pkts)
{
    channel_t *cp;
    fd_set m,f;
    int i, n, sel;
    struct timeval to;
    to.tv_sec  = 0;
    to.tv_usec = 1000;

    n = queue_length(channels);
    FD_ZERO(&m);
    for(i=0;i<n;i++) {
	cp = (channel_t*)queue_get(channels, i, Q_KEEP);
	FD_SET(cp->s,&m);
    }
    memcpy(&f,&m,sizeof(fd_set));
    while((sel = select(cp->s+1,&f,(fd_set*)0,(fd_set*)0, &to)) > 0) {
	for(i = 0; i < n; i++) {
	    cp = (channel_t*)queue_get(channels, i, Q_KEEP);
	    if (FD_ISSET(cp->s,&f)) {
		queue_pkt(cp,pkts);
	    }
	}
	memcpy(&f, &m, sizeof(fd_set));
    }
    if (sel==-1) 
	perror("select");
}

int
cmp(char *a, char *b)
{
    unsigned int *c,*d;

    c = (unsigned int*)a;
    d = (unsigned int*)b;

    if (*c<*d) return Q_CMP_LT;
    if (*c>*d) return Q_CMP_GT;
    return Q_CMP_EQ;
}

void
send_pkts(struct queue_s *pkts)
{
    struct sockaddr_in out;
    unsigned int now;
    unsigned int *addr;
    pkt_t *p;
    int i,n;

    now = get_ms();
    while((p=(pkt_t*)queue_get_lt(pkts, (char*)&now, Q_DETACH))) {
	assert(now > p->go_time);
	stats_add(p->channel->err_dist, 
		  now - p->go_time);
    
	if (drand48() > p->channel->loss) {
	    p->channel->forw++;
	    n = queue_length(p->channel->hosts);
	    for(i = 0;i < n; i++) {
		addr = (unsigned int*)queue_get(p->channel->hosts,i,Q_KEEP);
		if (mode==REFLECTOR || *addr!=p->src_addr) {
		    /*sned this packet */
		    memset(&out, 0, sizeof(out));
		    out.sin_family      = PF_INET;
		    out.sin_addr.s_addr = htonl(*addr);
		    out.sin_port        = htons(p->channel->port);
		    sendto(p->channel->s, p->data, p->size, 0, 
			   (struct sockaddr*)&out, sizeof(out));
		}
	    }
	} else {
	    p->channel->drop++;
	}
	free(p->data);
	free(p);
    }
}

int 
main(int argc, char *argv[])
{
    struct queue_s *control_socks;
    struct queue_s *channels;
    struct queue_s *pkts;
    channel_t *tc; 
    char      *tok, *known_hosts = NULL;
    char       ui_on = 1;
    char       loop  = 1;
    int        ac    = 0, control_fd = 0, control_port, i;

    if (argc == 1) {
	usage();
    }

    channels      = queue_create(cmp,0);
    pkts          = queue_create(cmp,0);
    control_socks = queue_create(cmp,0);

    mode = REFLECTOR;
    while(++ac < argc && argv[ac][0]=='-') {
	switch(argv[ac][1]) {
	case 'f':
	    mode = FORWARDER;
	    break;
	case 'n':
	    ui_on = 0;
	    break;
	case 'c':
	    if (++ac == argc) 
		usage();
	    control_port = atoi(argv[ac]);
	    init_control_sock(&control_fd, control_port);
	    break;
	case 'k':
	    if (++ac == argc)
		usage();
	    known_hosts = argv[ac];
	    break;
	default:
	    usage();
	}
    }
  
    while(ac < argc) {
	tc = (channel_t*)calloc(1,sizeof(channel_t));
	tc->hosts = queue_create(cmp,0);
	tc->port  = atoi(strtok(argv[ac],","));
	tc->err_dist = stats_create();
	if ((tok = strtok(NULL,","))) 
	    tc->loss = strtod(tok,NULL)/100;
	if ((tok = strtok(NULL,","))) 
	    tc->min_delay = tc->max_delay= atoi(tok);
	if ((tok = strtok(NULL,","))) 
	    tc->max_delay = atoi(tok);
	queue_insert(channels, (char*)tc);
	ac++;
    }

    if (queue_length(channels)) {
	init_reflect_sockets(channels);
	if (known_hosts) add_hosts(channels, known_hosts);
	if (ui_on) ui_init(channels);
	while(loop) {
	    read_pkts(channels, pkts);
	    send_pkts(pkts);
	    if (ui_on) {
		if (ui_update_needed == TRUE) {
		    ui_update();
		    ui_update_needed = FALSE;
		}
		loop = ui_process();
	    }
	    if (control_fd != 0) {
		process_control_cmds(control_fd, 
				     control_socks, 
				     channels);
	    }
	}
    }
        
    i = queue_length(channels);
    while(i-- > 0) {
	tc = (channel_t*)queue_get(channels, 0, Q_DETACH);
	printf("Stats for port %d:\nDropped %d / %d \nDelay Distn\n",
	       tc->port,
	       tc->drop,
	       tc->forw);
	stats_dump(tc->err_dist);
	stats_destroy(tc->err_dist);
	free(tc);
    }

    return 1;
}

 



