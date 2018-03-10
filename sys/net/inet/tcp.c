#include <net/inet/ip.h>
#include <net/inet/protohdr.h>
#include <net/inet/util.h>
#include <net/inet/params.h>
#include <net/util.h>
#include <kern/kernlib.h>
#include <kern/lock.h>
#include <kern/timer.h>
#include <kern/thread.h>
#include <kern/workqueue.h>
#include <net/inet/errno.h>

#define MOD(x,y) ((x) % (y))

#define tcp_header_len(th) ((th)->th_off*4)

#define NEED_PORT_ALLOC 0

#define STREAM_SEND_BUF 1024
#define STREAM_RECV_BUF 1024

#define SEQ2IDX_SEND(seq, cb) MOD((seq)-((cb)->iss+1), (cb)->snd_buf_len)

typedef int bool;
#define true 1
#define false 0

static mutex cblist_mtx;
static struct list_head tcpcb_list;
static struct workqueue *tcp_tx_wq;
static struct workqueue *tcp_timer_wq;

struct tcp_arrival {
  struct list_head link;
  u32 start_seq;
  u32 end_seq;
  struct list_head pkt_list;
};

#define arrival_len(a) ((a)->end_seq - (a)->start_seq + 1)
#define arrival_is_empty(a) ((a)->start_seq > (a)->end_seq)

struct tcpcb {
  struct sockaddr_in local_addr;
  struct sockaddr_in foreign_addr;
#define laddr local_addr.addr
#define lport local_addr.port
#define faddr foreign_addr.addr
#define fport foreign_addr.port

  u32 snd_unack; //未確認のシーケンス番号で、一番若いもの
  u32 snd_nxt; //次のデータ転送で使用できるシーケンス番号
  u16 snd_wnd; //送信ウィンドウサイズ
  u32 snd_wl1; //直近のウィンドウ更新に使用されたセグメントのシーケンス番号
  u32 snd_wl2; //直近のウィンドウ更新に使用されたセグメントの確認番号
  u32 snd_buf_used; //送信バッファに入っているデータのバイト数
  u32 snd_buf_len;
  bool snd_persisttim_enabled; //持続タイマが起動中か
  u32 iss; //初期送信シーケンス番号
  char *snd_buf;

  u32 rcv_nxt; //次のデータ転送で使用できるシーケンス番号
  u16 rcv_wnd; //受信ウィンドウサイズ
  u32 rcv_buf_len; //受信バッファサイズ
  u32 rcv_ack_cnt; //ACKを送るごとに1づつカウントアップしていく
  u32 rcv_fin; //FINを受信後にセットされる、FINのシーケンス番号
  u32 irs; //初期受信シーケンス番号

  struct list_head arrival_list; //到着パケット
  struct list_head timer_list;

  int myfin_state;
#define FIN_NOTREQUESTED 0
#define FIN_REQUESTED 1
#define FIN_SENT 2
#define FIN_ACKED 3
  u32 myfin_seq; //自身が送信したFINのシーケンス番号（ACKが来たか確かめる用に覚えとく）

  int state;
  u16 rtt;
  u16 mss;
  int opentype;
#define ACTIVE 0
#define PASSIVE 1
  int errno;

  struct queue_head cbqueue;
  struct list_head qlink;
  struct list_head link;

  bool is_userclosed; //close()が呼ばれたか。trueのときCLOSE状態に遷移したらソケット解放を行う
  mutex mtx;
};

#define TCP_STATE_CLOSED 0
#define TCP_STATE_LISTEN 1
#define TCP_STATE_SYN_RCVD 2
#define TCP_STATE_SYN_SENT 3
#define TCP_STATE_ESTABLISHED 4
#define TCP_STATE_FIN_WAIT_1 5
#define TCP_STATE_FIN_WAIT_2 6
#define TCP_STATE_CLOSING 7
#define TCP_STATE_TIME_WAIT 8
#define TCP_STATE_CLOSE_WAIT 9
#define TCP_STATE_LAST_ACK 10

static const char *TCP_STATE_STR[] = {
  "CLOSED",
  "LISTEN",
  "SYN_RCVD",
  "SYN_SENT",
  "ESTABLISHED",
  "FIN_WAIT_1",
  "FIN_WAIT_2",
  "CLOSING",
  "TIME_WAIT",
  "CLOSE_WAIT",
  "LAST_ACK",
};

#define TCP_OPT_END_OF_LIST 0
#define TCP_OPT_NOP 1
#define TCP_OPT_MSS 2
#define TCP_OPT_WIN_SCALE 3
#define TCP_OPT_S_ACK_PERM 4
#define TCP_OPT_S_ACK 5
#define TCP_OPT_TIMESTAMP 6


struct timinfo {
  union{
    struct{
      u32 start_seq;
      u32 end_seq;
      u32 is_zerownd_probe;
      struct pktbuf *pkt;
    } resend;
    struct{
      u32 seq;
    } delayack;
  } option;
#define resend_start_seq option.resend.start_seq
#define resend_end_seq option.resend.end_seq
#define resend_is_zerownd_probe option.resend.is_zerownd_probe
#define resend_pkt option.resend.pkt
#define delayack_seq option.delayack.seq
  int type;
#define TCP_TIMER_TYPE_REMOVED 0
#define TCP_TIMER_TYPE_FINACK 1
#define TCP_TIMER_TYPE_RESEND 2
#define TCP_TIMER_TYPE_TIMEWAIT 3
#define TCP_TIMER_TYPE_DELAYACK 4
  int msec;
  struct tcpcb *cb;
  struct list_head link;
};

struct timinfo *timinfo_new(int type);
void timinfo_free(struct timinfo *tinfo);
void tcp_timer_do(void *arg);

bool LE_LT(u32 a, u32 b, u32 c);
bool LT_LE(u32 a, u32 b, u32 c);
bool LE_LE(u32 a, u32 b, u32 c);
bool LT_LT(u32 a, u32 b, u32 c);

int is_used_port(in_port_t port);
in_port_t get_unused_port(void);

struct tcpcb *tcpcb_new(void);
void tcpcb_abort(struct tcpcb *cb);
void tcpcb_init(struct tcpcb *cb);
int tcpcb_get_prev_error(struct tcpcb *cb, int errno);
void tcpcb_reset(struct tcpcb *cb);
void tcpcb_alloc_buf(struct tcpcb *cb);
int tcpcb_has_arrival(struct tcpcb *cb);
void tcpcb_timer_add(struct tcpcb *cb, u32 msec, struct timinfo *tinfo);
void tcpcb_timer_remove_all(struct tcpcb *cb);

u32 tcp_geninitseq(void);
u16 tcp_checksum_recv(struct ip_hdr *ih, struct tcp_hdr *th);
u16 tcp_checksum_send(struct pktbuf *seg, in_addr_t ip_src, in_addr_t ip_dst);

void tcp_ctl_tx(u32 seq, u32 ack, u16 win, u8 flags, in_addr_t to_addr, in_port_t to_port, in_port_t my_port, struct tcpcb *cb);
void tcp_tx(void *arg UNUSED);
void tcp_tx_request(void);
struct pktbuf *sendbuf_to_pktbuf(struct tcpcb *cb, u32 from_index, u32 len);
void tcp_tx_from_buf(struct tcpcb *cb);
bool tcp_resend_from_buf(struct tcpcb *cb, struct timinfo *tinfo);
int tcp_write_to_sendbuf(struct tcpcb *cb, const char *data, u32 len);

void tcp_rx(struct pktbuf *pkt, struct ip_hdr *ih);
void tcp_rx_closed(struct pktbuf *pkt, struct ip_hdr *ih, struct tcp_hdr *th, u16 payload_len);
void tcp_rx_listen(struct pktbuf *pkt, struct ip_hdr *ih, struct tcp_hdr *th, struct tcpcb *cb);
void tcp_rx_synsent(struct pktbuf *pkt, struct ip_hdr *ih, struct tcp_hdr *th, struct tcpcb *cb);
void tcp_rx_otherwise(struct pktbuf *pkt, struct ip_hdr *ih, struct tcp_hdr *th, u16 payload_len, struct tcpcb *cb);

void tcp_arrival_free(struct tcp_arrival *a);
struct list_head arrival_pick_head(struct tcp_arrival *a, u32 len);
u32 arrival_copy_head(struct tcpcb *cb, char *base, u32 len);
void arrival_show(struct tcpcb *cb);
struct list_head arrival_pick_tail(struct tcp_arrival *a, u32 len);
void arrival_merge(struct tcp_arrival *to, struct tcp_arrival *from);
void tcp_arrival_add(struct pktbuf *pkt, struct tcpcb *cb, u32 start_seq, u32 end_seq);
int tcp_read_from_arrival(struct tcpcb *cb, char *data, u32 len);

void *tcp_sock_init(void);
int tcp_sock_connect(void *pcb, const struct sockaddr *addr);
int tcp_sock_listen(void *pcb, int backlog);
void *tcp_sock_accept(void *pcb, struct sockaddr *client_addr);
int tcp_sock_send(void *pcb, const char *msg, size_t len, int flags UNUSED);
int tcp_sock_recv(void *pcb, char *buf, size_t len, int flags UNUSED);
int tcp_sock_close(void *pcb);
int tcp_sock_bind(void *pcb, const struct sockaddr *addr);
int tcp_sock_sendto(void *pcb UNUSED, const char *msg UNUSED, size_t len UNUSED, int flags UNUSED, struct sockaddr *dest_addr UNUSED);
int tcp_sock_recvfrom(void *pcb UNUSED, char *buf UNUSED, size_t len UNUSED, int flags UNUSED, struct sockaddr *from_addr UNUSED);


static const struct socket_ops tcp_sock_ops = {
  .init = tcp_sock_init,
  .bind = tcp_sock_bind,
  .close = tcp_sock_close,
  .connect = tcp_sock_connect,
  .listen = tcp_sock_listen,
  .accept = tcp_sock_accept,
  .send = tcp_sock_send,
  .recv = tcp_sock_recv,
};


NET_INIT void tcp_init() {
  tcp_tx_wq = workqueue_new("tcp_tx workqueue");
  tcp_timer_wq = workqueue_new("tcp_timer workqueue");
  list_init(&tcpcb_list);
  mutex_init(&cblist_mtx);
  socket_register_ops(PF_INET, SOCK_STREAM, &tcp_sock_ops);
}

void tcp_stat(void) {
  struct list_head *p;
  mutex_lock(&cblist_mtx);

  puts("proto, local, foreign, state, sndwnd, rcvwnd");
  list_foreach(p, &tcpcb_list) {
    struct tcpcb *cb = list_entry(p, struct tcpcb, link);
    printf("tcp, %x:%u, %x:%u, %s, %u, %u\n", 
      cb->laddr, cb->lport, cb->faddr, cb->fport, 
      TCP_STATE_STR[cb->state], cb->snd_wnd, cb->rcv_wnd);
  }

  mutex_unlock(&cblist_mtx);
  return;
}

struct timinfo *timinfo_new(int type) {
  struct timinfo *ti = malloc(sizeof(struct timinfo));
  bzero(ti, sizeof(struct timinfo));
  ti->type = type;
  return ti;
}

void tcpcb_timer_add(struct tcpcb *cb, u32 msec, struct timinfo *tinfo) {
  tinfo->cb = cb;
  tinfo->msec = msec;
  list_pushback(&tinfo->link, &cb->timer_list);
  workqueue_add_delayed(tcp_timer_wq, tcp_timer_do, tinfo, msecs_to_ticks(msec));
}

void tcpcb_timer_remove_all(struct tcpcb *cb) {
  struct list_head *p, *tmp;
  mutex_lock(&cblist_mtx);
  list_foreach_safe(p, tmp, &cb->timer_list) {
    struct timinfo *tinfo = list_entry(p, struct timinfo, link);
    tinfo->type = TCP_TIMER_TYPE_REMOVED;
  }
  mutex_unlock(&cblist_mtx);
}


//mod 2^32で計算
bool LE_LT(u32 a, u32 b, u32 c) {
  if(a < c)
    return (a<=b) && (b<c);
  else if(a > c)
    return (a<=b) || (b<c);
  else
    return false;
}

bool LT_LE(u32 a, u32 b, u32 c) {
  if(a < c)
    return (a<b) && (b<=c);
  else if(a > c)
    return (a<b) || (b<=c);
  else
    return false;
}

bool LE_LE(u32 a, u32 b, u32 c) {
  if(a < c)
    return (a<=b) && (b<=c);
  else if(a > c)
    return (a<=b) || (b<=c);
  else
    return a==b;
}

bool LT_LT(u32 a, u32 b, u32 c) {
  if(a < c)
    return (a<b) && (b<c);
  else if(a > c)
    return (a<b) || (b<c);
  else
    return false;
}

int is_used_port(in_port_t port) {
  struct list_head *p;
  list_foreach(p, &tcpcb_list) {
    struct tcpcb *cb = list_entry(p, struct tcpcb, link);
    if(cb->lport == port)
      return 1;
  }

  return 0;
}

in_port_t get_unused_port() {
	for(in_port_t p=49152; p<65535; p++)
		if(!is_used_port(p))
			return p;

	return 0;
}


void tcpcb_abort(struct tcpcb *cb) {
  switch(cb->state){
  case TCP_STATE_SYN_RCVD:
  case TCP_STATE_ESTABLISHED:
  case TCP_STATE_FIN_WAIT_1:
  case TCP_STATE_FIN_WAIT_2:
  case TCP_STATE_CLOSE_WAIT:
    tcp_ctl_tx(cb->snd_nxt, 0, 0, TH_RST,
      cb->faddr, cb->fport, cb->lport, NULL);
  }
  tcpcb_reset(cb);
}

void tcpcb_init(struct tcpcb *cb) {
  int errno = cb->errno;
  memset(cb, 0, sizeof(struct tcpcb));

  cb->errno = errno;

  mutex_init(&cb->mtx);

  cb->is_userclosed = false;
  cb->snd_persisttim_enabled = false;

  cb->state = TCP_STATE_CLOSED;
  cb->rtt = TCP_RTT_INIT;
  cb->myfin_state = FIN_NOTREQUESTED;

  cb->rcv_buf_len = STREAM_RECV_BUF;
  cb->snd_buf_len = STREAM_SEND_BUF;

  cb->snd_buf = NULL;
  list_init(&cb->arrival_list);
  list_init(&cb->timer_list);
  queue_init(&cb->cbqueue, 0);
}

int tcpcb_get_prev_error(struct tcpcb *cb, int errno) {
  return (cb->errno < 0) ? cb->errno : errno;
}

void tcp_arrival_free(struct tcp_arrival *a) {
  list_free_all(&a->pkt_list, struct pktbuf, link, pktbuf_free);
  free(a);
}

void tcpcb_reset(struct tcpcb *cb) {
  struct list_head *p;
  while((p = queue_dequeue(&cb->cbqueue)) != NULL) {
    //FIXME: 排他制御
    struct tcpcb *b = list_entry(p, struct tcpcb, qlink);
    b->is_userclosed = true;
    tcpcb_abort(b);
  }

  if(cb->snd_buf != NULL)
    free(cb->snd_buf);

  list_free_all(&cb->arrival_list, struct tcp_arrival, link, tcp_arrival_free);
  tcpcb_timer_remove_all(cb);
  list_remove(&cb->link);

  if(cb->is_userclosed)
    free(cb);
  else
    tcpcb_init(cb);

  return;
}

struct tcpcb *tcpcb_new() {
  struct tcpcb *cb = malloc(sizeof(struct tcpcb));
  bzero(cb, sizeof(struct tcpcb));
  tcpcb_init(cb);
  return cb;
}

void tcpcb_alloc_buf(struct tcpcb *cb) {
  cb->snd_wl1 = 0;
  cb->snd_wl2 = 0;
  cb->snd_buf = malloc(cb->snd_buf_len);
  cb->rcv_wnd = cb->rcv_buf_len;
  return;
}

u32 tcp_geninitseq() {
  //return systim + 64000;
  return 64000;
}

u16 tcp_checksum_recv(struct ip_hdr *ih, struct tcp_hdr *th) {
  struct tcp_pseudo_hdr pseudo;
  pseudo.tp_src = ih->ip_src;
  pseudo.tp_dst = ih->ip_dst;
  pseudo.tp_type = 6;
  pseudo.tp_void = 0;
  pseudo.tp_len = hton16(ntoh16(ih->ip_len) - ip_header_len(ih)); //TCPヘッダ+TCPペイロードの長さ

  return checksum2((u16*)(&pseudo), (u16*)th, sizeof(struct tcp_pseudo_hdr), ntoh16(ih->ip_len) - ip_header_len(ih));
}

u16 tcp_checksum_send(struct pktbuf *seg, in_addr_t ip_src, in_addr_t ip_dst) {
  int segsize = pktbuf_get_size(seg);
  struct tcp_pseudo_hdr pseudo;
  pseudo.tp_src = ip_src;
  pseudo.tp_dst = ip_dst;
  pseudo.tp_type = 6;
  pseudo.tp_void = 0;
  pseudo.tp_len = hton16(segsize); //TCPヘッダ+TCPペイロードの長さ

  return checksum2((u16*)(&pseudo), (u16*)seg->head, sizeof(struct tcp_pseudo_hdr), segsize);
}

//制御用のセグメント（ペイロードなし）を送信
void tcp_ctl_tx(u32 seq, u32 ack, u16 win, u8 flags, in_addr_t to_addr, in_port_t to_port, in_port_t my_port, struct tcpcb *cb) {
  in_addr_t r_src, r_dst;
  devno_t devno;
  if(ip_routing(cb?cb->laddr:INADDR_ANY, to_addr, &r_src, &r_dst, &devno))
    return; //no interface to send

  struct pktbuf *tcpseg = pktbuf_alloc(MAX_HDRLEN_TCP);

  pktbuf_reserve_headroom(tcpseg, MAX_HDRLEN_TCP);

  struct tcp_hdr *th = (struct tcp_hdr *)pktbuf_add_header(tcpseg, sizeof(struct tcp_hdr));
  th->th_sport = my_port;
  th->th_dport = to_port;
  th->th_seq = hton32(seq);
  th->th_ack = hton32(ack);
  th->th_flags = flags;
  th->th_x2 = 0;
  th->th_win = hton16(win);
  th->th_sum = 0;
  th->th_urp = 0;
  th->th_off = (sizeof(struct tcp_hdr)) / 4;
  th->th_sum = tcp_checksum_send(tcpseg, r_src, r_dst);

  if(flags & TH_ACK && cb!=NULL)
    cb->rcv_ack_cnt++;

  struct timinfo *tinfo;
  if(cb != NULL){
    tinfo = timinfo_new(TCP_TIMER_TYPE_RESEND);
    tinfo->resend_start_seq = seq;
    tinfo->resend_end_seq = seq;
    tinfo->resend_pkt = tcpseg;
  }
  ip_tx(tcpseg, r_src, r_dst, IPTYPE_TCP);
  

  if(cb != NULL)
    tcpcb_timer_add(cb, cb->rtt*TCP_TIMER_UNIT, tinfo);
}


void tcp_rx_closed(struct pktbuf *pkt, struct ip_hdr *ih, struct tcp_hdr *th, u16 payload_len) {
  if(!(th->th_flags & TH_RST)){
    if(th->th_flags & TH_ACK){
      tcp_ctl_tx(0, ntoh32(th->th_seq)+payload_len, 0, TH_ACK|TH_RST, ih->ip_src, th->th_sport, th->th_dport, NULL);
    }else
      tcp_ctl_tx(ntoh32(th->th_ack), 0, 0, TH_RST, ih->ip_src, th->th_sport, th->th_dport, NULL);
  }
  pktbuf_free(pkt);
}

void tcp_rx_listen(struct pktbuf *pkt, struct ip_hdr *ih, struct tcp_hdr *th, struct tcpcb *cb) {
  //already locked
  if(th->th_flags & TH_RST){
    goto exit;
  }
  if(th->th_flags & TH_ACK){
    tcp_ctl_tx(ntoh32(th->th_ack), 0, 0, TH_RST, ih->ip_src, th->th_sport, th->th_dport, NULL);
    goto exit;
  }
  if(th->th_flags & TH_SYN){
    struct tcpcb *newcb;
    if(!queue_is_full(&cb->cbqueue)) {
      newcb = tcpcb_new();
      queue_enqueue(&newcb->qlink, &cb->cbqueue);
    } else {
      goto exit;
    }

    newcb->laddr = cb->laddr;
    newcb->lport = cb->lport;

    newcb->faddr = ih->ip_src;
    newcb->fport = th->th_sport;

    newcb->state = TCP_STATE_LISTEN;

    newcb->rcv_nxt = ntoh32(th->th_seq)+1;
    newcb->irs = ntoh32(th->th_seq);

    newcb->snd_wnd = MIN(ntoh16(th->th_win), cb->snd_buf_len);
    newcb->snd_wl1 = ntoh32(th->th_seq);
    newcb->snd_wl2 = ntoh32(th->th_ack);

    newcb->mss = MSS;
    
    mutex_lock(&cblist_mtx);
    list_pushback(&newcb->link, &tcpcb_list);
    mutex_unlock(&cblist_mtx);
    thread_wakeup(cb);
  }

exit:
  pktbuf_free(pkt);
  return;
}

void tcp_rx_synsent(struct pktbuf *pkt, struct ip_hdr *ih, struct tcp_hdr *th, struct tcpcb *cb) {
  //already locked
  if(th->th_flags & TH_ACK){
    if(ntoh32(th->th_ack) != cb->iss+1){
      if(!(th->th_flags & TH_RST)){
        tcp_ctl_tx(ntoh32(th->th_ack), 0, 0, TH_RST, ih->ip_src, th->th_sport, th->th_dport, NULL);
      }
      goto exit;
    }else{
      cb->snd_unack++;
    }
  }
  if(th->th_flags & TH_RST){
    if(LE_LE(cb->snd_unack, ntoh32(th->th_ack), cb->snd_nxt)){
      tcpcb_reset(cb);
      thread_wakeup(cb);
    }
    goto exit;
  }
  if(th->th_flags & TH_SYN){
    cb->rcv_nxt = ntoh32(th->th_seq)+1;

    cb->irs = ntoh32(th->th_seq);
    if(th->th_flags & TH_ACK)
      cb->snd_unack = ntoh32(th->th_ack);
    if(cb->snd_unack == cb->iss){
      cb->state = TCP_STATE_SYN_RCVD;
      tcp_ctl_tx(cb->iss, cb->rcv_nxt, 0, TH_SYN|TH_ACK, ih->ip_src, th->th_sport, th->th_dport, cb);
    }else{
      cb->snd_wnd = MIN(ntoh16(th->th_win), cb->snd_buf_len);
      cb->snd_wl1 = ntoh32(th->th_seq);
      cb->snd_wl2 = ntoh32(th->th_ack);
      cb->mss = MSS;
      tcpcb_alloc_buf(cb);
      cb->state = TCP_STATE_ESTABLISHED;
      tcp_ctl_tx(cb->snd_nxt, cb->rcv_nxt, cb->rcv_wnd, TH_ACK, ih->ip_src, th->th_sport, th->th_dport, NULL);
      thread_wakeup(cb);
    }
  }

exit:
  pktbuf_free(pkt);
  return;
}

struct list_head arrival_pick_head(struct tcp_arrival *a, size_t len) {
  struct list_head saved;
  struct list_head *p, *tmp;

  list_init(&saved);

  list_foreach_safe(p, tmp, &a->pkt_list){
    if(len == 0)
      break;
    list_remove(p);
    list_pushback(p, &saved);
    struct pktbuf *pkt = list_entry(p, struct pktbuf, link);
    if(pktbuf_get_size(pkt) > len)
      pkt->tail -= pktbuf_get_size(pkt) - len;
    len -= MIN(pktbuf_get_size(pkt), len);
    a->start_seq += MIN(pktbuf_get_size(pkt), len);
  }

  return saved;
}

void arrival_show(struct tcpcb *cb) {
  struct list_head *p, *q, *tmp1, *tmp2;
puts("------------");
  list_foreach_safe(p, tmp1, &cb->arrival_list) {
    struct tcp_arrival *a = list_entry(p, struct tcp_arrival, link);
    printf("arrival: %u - %u\n", a->start_seq, a->end_seq);
    list_foreach_safe(q, tmp2, &a->pkt_list){
      struct pktbuf *pkt = list_entry(q, struct pktbuf, link);
      printf("\tsize: %u\n", pktbuf_get_size(pkt));
    }
  }
puts("------------");
}

u32 arrival_copy_head(struct tcpcb *cb, char *base, size_t len) {
  u32 remain = len;
  struct list_head *p, *q, *tmp1, *tmp2;

  list_foreach_safe(p, tmp1, &cb->arrival_list) {
    struct tcp_arrival *a = list_entry(p, struct tcp_arrival, link);
    list_foreach_safe(q, tmp2, &a->pkt_list){
      if(remain == 0)
        goto exit;
      struct pktbuf *pkt = list_entry(q, struct pktbuf, link);
      u32 copied = MIN(pktbuf_get_size(pkt), remain);
      memcpy(base, pkt->head, copied);
      remain -= copied;
      base += copied;
      a->start_seq += copied;

      if(copied < pktbuf_get_size(pkt)) {
        pkt->head += copied;
        goto exit;
      }else {
        list_remove(&pkt->link);
        pktbuf_free(pkt);
      }
    }
    list_remove(&a->link);
    tcp_arrival_free(a);
  }

exit:
  return len - remain; 
}

struct list_head arrival_pick_tail(struct tcp_arrival *a, size_t len) {
  struct list_head saved;
  struct list_head *p, *tmp;

  list_init(&saved);

  list_foreach_safe_reverse(p, tmp, &a->pkt_list){
    if(len == 0)
      break;
    list_remove(p);
    list_pushfront(p, &saved);
    struct pktbuf *pkt = list_entry(p, struct pktbuf, link);
    if(pktbuf_get_size(pkt) > len)
      pkt->head += pktbuf_get_size(pkt) - len;
    len -= MIN(pktbuf_get_size(pkt), len);
    a->end_seq -= MIN(pktbuf_get_size(pkt), len);
  }

  return saved;
}

void arrival_merge(struct tcp_arrival *to, struct tcp_arrival *from) {
  if(LT_LT(from->start_seq, to->start_seq, to->end_seq)) {
    //先頭を拡張
    u32 expand_len = to->start_seq - from->start_seq;
    struct list_head pkts = arrival_pick_head(from, expand_len);
    list_append_front(&to->pkt_list, &pkts);
    to->start_seq -= expand_len;
  }
  if(LT_LT(to->start_seq, to->end_seq, from->end_seq)) {
    //末尾を拡張
    u32 expand_len = from->end_seq - to->end_seq;
    struct list_head pkts = arrival_pick_tail(from, expand_len);
    list_append_back(&to->pkt_list, &pkts);
    to->end_seq += expand_len;
  }
}

void tcp_arrival_add(struct pktbuf *pkt, struct tcpcb *cb, u32 start_seq, u32 end_seq) {
  struct tcp_arrival *newa = malloc(sizeof(struct tcp_arrival));
  newa->start_seq = start_seq;
  newa->end_seq = end_seq;
  list_init(&newa->pkt_list);
  list_pushfront(&pkt->link, &newa->pkt_list);
printf("tcp_arrival: seq# %u to %u  rcv_nxt=%u\n", start_seq, end_seq, cb->rcv_nxt);
  struct list_head *p, *tmp;
  list_foreach_safe(p, tmp, &cb->arrival_list){
    struct tcp_arrival *a = list_entry(p, struct tcp_arrival, link);
    if(LE_LE(newa->start_seq, a->start_seq, newa->end_seq+1) 
         || LE_LE(newa->start_seq-1, a->end_seq, newa->end_seq)) {
      list_remove(p);
      arrival_merge(newa, a);
      tcp_arrival_free(a);
    } else if(LT_LT(newa->end_seq, a->start_seq, a->end_seq)) {
      break;
    }
  }

  list_pushfront(&newa->link, p);

  //arrival_show(cb);
  struct tcp_arrival *head = list_entry(list_first(&cb->arrival_list), struct tcp_arrival, link);
  if(LE_LE(head->start_seq, cb->rcv_nxt, head->end_seq)) {
    u32 hlen = head->end_seq - head->start_seq + 1;
    cb->rcv_nxt = head->end_seq + 1;
printf("tcp_arrival: rcv_nxt=%u\n", cb->rcv_nxt);
    cb->rcv_wnd = cb->rcv_buf_len - hlen;
    thread_wakeup(cb);
  }
}

int tcpcb_has_arrival(struct tcpcb *cb) {
  return (cb->rcv_wnd < cb->rcv_buf_len);
}

struct pktbuf *sendbuf_to_pktbuf(struct tcpcb *cb, u32 from_index, size_t len) {
  struct pktbuf *tcpseg = pktbuf_alloc(MAX_HDRLEN_TCP + len);
  pktbuf_reserve_headroom(tcpseg, MAX_HDRLEN_TCP);

  if(cb->snd_buf_len - from_index >= len){
    //折り返さない
    pktbuf_copyin(tcpseg, &(cb->snd_buf[from_index]), len, 0);
  }else{
    //折り返す
    pktbuf_copyin(tcpseg, &(cb->snd_buf[from_index]), cb->snd_buf_len-from_index, 0);
    pktbuf_copyin(tcpseg, cb->snd_buf, len - (cb->snd_buf_len-from_index), 0);
  }
  return tcpseg;
}


void tcp_tx_from_buf(struct tcpcb *cb) {
  in_addr_t r_src, r_dst;
  devno_t devno;
  if(ip_routing(cb->laddr, cb->faddr, &r_src, &r_dst, &devno))
    return; //no interface to send

  bool is_zerownd_probe = false;

  struct tcp_hdr th_base;
  th_base.th_dport = cb->fport;
  th_base.th_flags = TH_ACK;
  th_base.th_off = sizeof(struct tcp_hdr)/4;
  th_base.th_sport = cb->lport;
  th_base.th_sum = 0;
  th_base.th_urp = 0;
  th_base.th_x2 = 0;

  if(cb->snd_persisttim_enabled == false && cb->snd_wnd == 0){
    if(LE_LT(cb->snd_unack, cb->snd_nxt, cb->snd_unack+cb->snd_buf_used)){
      //1byte以上の送信可能なデータが存在
      is_zerownd_probe = true;
      cb->snd_persisttim_enabled = true;
    }
  }

  while(is_zerownd_probe ||
    (cb->snd_buf_used>0 &&
    !LE_LT(cb->snd_unack,
            cb->snd_unack+cb->snd_buf_used-1, cb->snd_nxt))){
    int payload_len_all;
    if(LE_LT(cb->snd_nxt, cb->snd_unack+cb->snd_buf_used, cb->snd_nxt+cb->snd_wnd))
      payload_len_all = cb->snd_unack+cb->snd_buf_used - cb->snd_nxt;
    else
      payload_len_all = cb->snd_nxt+cb->snd_wnd - cb->snd_nxt;
    int payload_len = MIN(cb->mss, payload_len_all);

    if(is_zerownd_probe) payload_len = 1;
    if(payload_len == 0) break;

    struct timinfo *tinfo = timinfo_new(TCP_TIMER_TYPE_RESEND);
    tinfo->resend_start_seq = cb->snd_nxt;
    tinfo->resend_end_seq = cb->snd_nxt + payload_len -1;
    tinfo->resend_is_zerownd_probe = is_zerownd_probe;

    struct pktbuf *tcpseg = sendbuf_to_pktbuf(cb, SEQ2IDX_SEND(cb->snd_nxt, cb), payload_len);
    tinfo->resend_pkt = tcpseg;
    pktbuf_add_header(tcpseg, sizeof(struct tcp_hdr));
    pktbuf_copyin(tcpseg, (char *)&th_base, sizeof(struct tcp_hdr), 0);

    struct tcp_hdr *th = (struct tcp_hdr*)tcpseg->head;
    th->th_seq = hton32(cb->snd_nxt);
    th->th_ack = hton32(cb->rcv_nxt);
    th->th_win = hton16(cb->rcv_wnd);
    th->th_sum = tcp_checksum_send(tcpseg, r_src, r_dst);

    cb->rcv_ack_cnt++;

    if(!is_zerownd_probe){
      cb->snd_nxt += payload_len;
      cb->snd_wnd -= payload_len;
    }

    ip_tx(tcpseg, r_src, r_dst, IPTYPE_TCP);

    tcpcb_timer_add(cb, cb->rtt*TCP_TIMER_UNIT, tinfo);

    is_zerownd_probe = false;
  }

  if(cb->snd_wnd > 0 && cb->myfin_state == FIN_REQUESTED) {
    //送信ウィンドウが0でなく、かつここまで降りてきてFIN送信要求が出ているということは送信すべきものがもう無いのでFINを送って良い
    cb->myfin_state = FIN_SENT;
    cb->myfin_seq = cb->snd_nxt;
    cb->snd_nxt++;

    tcp_ctl_tx(cb->myfin_seq, cb->rcv_nxt, cb->rcv_wnd, TH_FIN|TH_ACK, cb->faddr, cb->fport, cb->lport, cb);

    if(cb->state == TCP_STATE_CLOSE_WAIT)
      cb->state = TCP_STATE_LAST_ACK;
  }

  return;
}

bool tcp_resend_from_buf(struct tcpcb *cb, struct timinfo *tinfo) {
  if(!LE_LT(cb->snd_unack, tinfo->resend_end_seq, cb->snd_nxt+cb->snd_wnd))
    return false; //送信可能なものはない

  in_addr_t r_src, r_dst;
  devno_t devno;
  if(ip_routing(cb->laddr, cb->faddr, &r_src, &r_dst, &devno))
    return false; //no interface to send

  struct pktbuf *pkt = tinfo->resend_pkt;
  struct tcp_hdr *th = (struct tcp_hdr *)pkt->head;
  th->th_sum = 0;
  th->th_ack = hton32(cb->rcv_nxt);
  th->th_flags |= TH_ACK;
  th->th_win = hton16(cb->rcv_wnd);
  th->th_sum = tcp_checksum_send(pkt, r_src, cb->faddr);

  cb->rcv_ack_cnt++;

  ip_tx(pkt, r_src, r_dst, IPTYPE_TCP);

  return true;
}

int tcp_write_to_sendbuf(struct tcpcb *cb, const char *data, u32 len) {
  u32 remain = len;
  while(remain > 0) {
    if(cb->snd_buf_used < cb->snd_buf_len){
      u32 write_start_index = SEQ2IDX_SEND(cb->snd_unack+cb->snd_buf_used, cb);
      u32 write_len = MIN(remain, cb->snd_buf_len - write_start_index);
      memcpy(&(cb->snd_buf[write_start_index]), data, write_len);
      remain -= write_len;
      cb->snd_buf_used += write_len;
      data += write_len;
    }else{
      mutex_unlock(&cb->mtx);
      tcp_tx_request();
      thread_sleep(cb);
      mutex_lock(&cb->mtx);
      switch(cb->state){
      case TCP_STATE_CLOSED:
      case TCP_STATE_LISTEN:
      case TCP_STATE_SYN_SENT:
      case TCP_STATE_SYN_RCVD:
        return tcpcb_get_prev_error(cb, ECONNNOTEXIST);
      case TCP_STATE_FIN_WAIT_1:
      case TCP_STATE_FIN_WAIT_2:
      case TCP_STATE_CLOSING:
      case TCP_STATE_LAST_ACK:
      case TCP_STATE_TIME_WAIT:
        return tcpcb_get_prev_error(cb, ECONNCLOSING);
      }
    }
  }
  return len - remain;
}

int tcp_read_from_arrival(struct tcpcb *cb, char *data, u32 len) {
  u32 copied = 0;

  while(copied == 0) {
    if(!tcpcb_has_arrival(cb)) {
      switch(cb->state){
      case TCP_STATE_ESTABLISHED:
      case TCP_STATE_FIN_WAIT_1:
      case TCP_STATE_FIN_WAIT_2:
        break;
      default:
        return tcpcb_get_prev_error(cb, ECONNCLOSING);
      }
    } else {
      copied = arrival_copy_head(cb, data, len);
      cb->rcv_wnd += copied;
      break;
    }

    mutex_unlock(&cb->mtx);
    thread_sleep(cb);
    mutex_lock(&cb->mtx);

    switch(cb->state){
    case TCP_STATE_CLOSED:
    case TCP_STATE_LISTEN:
    case TCP_STATE_SYN_SENT:
    case TCP_STATE_SYN_RCVD:
      return tcpcb_get_prev_error(cb, ECONNNOTEXIST);
    case TCP_STATE_CLOSING:
    case TCP_STATE_LAST_ACK:
    case TCP_STATE_TIME_WAIT:
      return tcpcb_get_prev_error(cb, ECONNCLOSING);
    }
  }

  return copied;
}


void tcp_rx_otherwise(struct pktbuf *pkt, struct ip_hdr *ih, struct tcp_hdr *th, u16 payload_len, struct tcpcb *cb) {
  //already locked
  int pkt_keep = 0;

  if(payload_len == 0 && cb->rcv_wnd == 0){
    if(ntoh32(th->th_seq) != cb->rcv_nxt)
      goto cantrecv;
  }else if(payload_len == 0 && cb->rcv_wnd > 0){
    if(!LE_LT(cb->rcv_nxt, ntoh32(th->th_seq), cb->rcv_nxt+cb->rcv_wnd))
      goto cantrecv;
  }else if(payload_len > 0 && cb->rcv_wnd == 0){
    goto cantrecv;
  }else if(payload_len > 0 && cb->rcv_wnd > 0){
    if(!(LE_LT(cb->rcv_nxt, ntoh32(th->th_seq), cb->rcv_nxt+cb->rcv_wnd) ||
        LE_LT(cb->rcv_nxt, ntoh32(th->th_seq)+payload_len-1, cb->rcv_nxt+cb->rcv_wnd)))
      goto cantrecv;
  }

  if(th->th_flags & TH_RST){
    switch(cb->state){
    case TCP_STATE_SYN_RCVD:
      if(cb->opentype == PASSIVE){
        cb->state = TCP_STATE_LISTEN;
      }else{
        tcpcb_reset(cb);
        cb->errno = ECONNREFUSED;
        thread_wakeup(cb);
      }
      goto exit;
      break;
    case TCP_STATE_ESTABLISHED:
    case TCP_STATE_FIN_WAIT_1:
    case TCP_STATE_FIN_WAIT_2:
    case TCP_STATE_CLOSE_WAIT:
      tcpcb_reset(cb);
      cb->errno = ECONNRESET;
      thread_wakeup(cb);
      goto exit;
      break;
    case TCP_STATE_CLOSING:
    case TCP_STATE_LAST_ACK:
    case TCP_STATE_TIME_WAIT:
      tcpcb_reset(cb);
      goto exit;
      break;
    }
  }

  if(th->th_flags & TH_SYN){
    switch(cb->state){
    case TCP_STATE_SYN_RCVD:
      if(cb->opentype == PASSIVE){
        cb->state = TCP_STATE_LISTEN;
        goto exit;
      }
    case TCP_STATE_ESTABLISHED:
    case TCP_STATE_FIN_WAIT_1:
    case TCP_STATE_FIN_WAIT_2:
    case TCP_STATE_CLOSE_WAIT:
    case TCP_STATE_CLOSING:
    case TCP_STATE_LAST_ACK:
    case TCP_STATE_TIME_WAIT:
      tcpcb_reset(cb);
      cb->errno = ECONNRESET;
      thread_wakeup(cb);
      tcp_ctl_tx(ntoh32(th->th_ack), cb->rcv_nxt, 0, TH_RST, ih->ip_src, th->th_sport, th->th_dport, NULL);
      goto exit;
      break;
    }
  }

  if(th->th_flags & TH_ACK){
    switch(cb->state){
    case TCP_STATE_SYN_RCVD:
      if(LE_LE(cb->snd_unack, hton32(th->th_ack), cb->snd_nxt)){
        cb->snd_wnd = MIN(ntoh16(th->th_win), cb->snd_buf_len);
        cb->snd_wl1 = ntoh32(th->th_seq);
        cb->snd_wl2 = ntoh32(th->th_ack);
        cb->snd_unack = hton32(th->th_ack);

        tcpcb_alloc_buf(cb);

        cb->state = TCP_STATE_ESTABLISHED;
        thread_wakeup(cb);
      }else{
        tcp_ctl_tx(ntoh32(th->th_ack), cb->rcv_nxt, 0, TH_RST, ih->ip_src, th->th_sport, th->th_dport, NULL);
        goto exit;
      }
    case TCP_STATE_ESTABLISHED:
    case TCP_STATE_FIN_WAIT_1:
    case TCP_STATE_FIN_WAIT_2:
    case TCP_STATE_CLOSE_WAIT:
    case TCP_STATE_CLOSING:
      if(LT_LE(cb->snd_unack, ntoh32(th->th_ack), cb->snd_nxt)){
        u32 unack_seq_before = cb->snd_unack;
        cb->snd_unack = ntoh32(th->th_ack);

        if(cb->snd_unack >= unack_seq_before)
          cb->snd_buf_used -= cb->snd_unack - unack_seq_before;
        else
          cb->snd_buf_used = (0xffffffff - unack_seq_before + 1) + cb->snd_unack;
      }else if(ntoh32(th->th_ack) != cb->snd_unack){
        tcp_ctl_tx(cb->snd_nxt, cb->rcv_nxt, cb->rcv_wnd, TH_ACK, ih->ip_src, th->th_sport, th->th_dport, NULL);
        goto exit;
      }
      if(LE_LE(cb->snd_unack, ntoh32(th->th_ack), cb->snd_nxt)){
        if(LT_LE(cb->snd_wl1, ntoh32(th->th_seq), cb->rcv_nxt+cb->rcv_wnd) ||
           (cb->snd_wl1==ntoh32(th->th_seq)
            && LE_LE(cb->snd_wl2, ntoh32(th->th_ack), cb->snd_nxt))){
          cb->snd_wnd = MIN(ntoh16(th->th_win), cb->snd_buf_len);
          cb->snd_wl1 = ntoh32(th->th_seq);
          cb->snd_wl2 = ntoh32(th->th_ack);
        }
      }
      if(cb->state == TCP_STATE_FIN_WAIT_1){
        if(cb->myfin_state== FIN_ACKED || (cb->myfin_state == FIN_SENT && ntoh32(th->th_ack)-1 == cb->myfin_seq)){
          cb->state = TCP_STATE_FIN_WAIT_2;
          cb->myfin_state = FIN_ACKED;
        }
      }
      /*if(cb->state == TCP_STATE_FIN_WAIT_2){
        if(cb->send_used_len == 0){

        }
      }*/
      if(cb->state == TCP_STATE_CLOSING){
        if(cb->myfin_state== FIN_ACKED || (cb->myfin_state == FIN_SENT && ntoh32(th->th_ack)-1 == cb->myfin_seq)){
          cb->state = TCP_STATE_TIME_WAIT;
          cb->myfin_state = FIN_ACKED;
        }else{
          goto exit;
        }
      }
      break;
    case TCP_STATE_LAST_ACK:
      if(cb->myfin_state== FIN_ACKED || (cb->myfin_state == FIN_SENT && ntoh32(th->th_ack)-1 == cb->myfin_seq)){
        tcpcb_reset(cb);
        goto exit;
      }
      break;
    case TCP_STATE_TIME_WAIT:
      break;
    }
  }else{
    goto exit;
  }

  if(payload_len > 0){
    switch(cb->state){
    case TCP_STATE_ESTABLISHED:
    case TCP_STATE_FIN_WAIT_1:
    case TCP_STATE_FIN_WAIT_2:
      pktbuf_remove_header(pkt, tcp_header_len(th));
      tcp_arrival_add(pkt, cb, ntoh32(th->th_seq), ntoh32(th->th_seq) + payload_len - 1);
      pkt_keep = 1;
      //遅延ACKタイマ開始
      struct timinfo *tinfo = timinfo_new(TCP_TIMER_TYPE_DELAYACK);
      tinfo->delayack_seq = cb->rcv_ack_cnt;
      tcpcb_timer_add(cb, TCP_DELAYACK_TIME, tinfo);
      break;
    }
  }

  if(th->th_flags & TH_FIN){
    switch(cb->state){
    case TCP_STATE_CLOSED:
    case TCP_STATE_LISTEN:
    case TCP_STATE_SYN_SENT:
      goto exit;
      break;
    }

    /*
    if(ntoh32(th->th_seq) != cb->rcv_nxt)
      goto exit;
    */
    //相手からFINが送られてきたということは、こちらに全てのセグメントが到着している
    cb->rcv_fin = ntoh32(th->th_seq);
    cb->rcv_nxt = cb->rcv_fin+1;
    tcp_ctl_tx(cb->snd_nxt, cb->rcv_nxt, cb->rcv_wnd, TH_ACK, 
      ih->ip_src, th->th_sport, th->th_dport, NULL);

    switch(cb->state){
    case TCP_STATE_SYN_RCVD:
    case TCP_STATE_ESTABLISHED:
      cb->state = TCP_STATE_CLOSE_WAIT;
      thread_wakeup(cb);
      break;
    case TCP_STATE_FIN_WAIT_1:
      if(cb->myfin_state== FIN_ACKED || (cb->myfin_state == FIN_SENT && ntoh32(th->th_ack)-1 == cb->myfin_seq)){
        cb->state = TCP_STATE_TIME_WAIT;
        cb->myfin_state = FIN_ACKED;
        tcpcb_timer_remove_all(cb);
        tcpcb_timer_add(cb, TCP_TIMEWAIT_TIME, timinfo_new(TCP_TIMER_TYPE_TIMEWAIT));
      }else{
        cb->state = TCP_STATE_CLOSING;
      }
      break;
    case TCP_STATE_FIN_WAIT_2:
      cb->state = TCP_STATE_TIME_WAIT;
      tcpcb_timer_remove_all(cb);
      tcpcb_timer_add(cb, TCP_TIMEWAIT_TIME, timinfo_new(TCP_TIMER_TYPE_TIMEWAIT));
      break;
    case TCP_STATE_TIME_WAIT:
      tcpcb_timer_remove_all(cb);
      tcpcb_timer_add(cb, TCP_TIMEWAIT_TIME, timinfo_new(TCP_TIMER_TYPE_TIMEWAIT));
      break;
    }
  }
  goto exit;

cantrecv:
printf("can't receive. %d/%d\n", payload_len, cb->rcv_wnd);
  //ACK送信
  if(!(th->th_flags & TH_RST)){
    tcp_ctl_tx(cb->snd_nxt, cb->rcv_nxt, cb->rcv_wnd, TH_ACK, ih->ip_src, th->th_sport, th->th_dport, NULL);
  }

exit:
  if(!pkt_keep)
    pktbuf_free(pkt);
  return;
}

void tcp_rx(struct pktbuf *pkt, struct ip_hdr *ih) {
  struct tcp_hdr *th = (struct tcp_hdr *)pkt->head;

  u32 msgsize = pktbuf_get_size(pkt);

  if(msgsize < sizeof(struct tcp_hdr) ||
    msgsize < tcp_header_len(th)){
    goto exit;
  }

  if(tcp_checksum_recv(ih, th) != 0)
    goto exit;

  u16 payload_len = msgsize - tcp_header_len(th);

  struct tcpcb *cb = NULL;
  struct tcpcb *listener = NULL;
  struct list_head *p;
  mutex_lock(&cblist_mtx);

  list_foreach(p, &tcpcb_list) {
    struct tcpcb *b = list_entry(p, struct tcpcb, link);
    if(b->lport == th->th_dport) {
      mutex_lock(&b->mtx);
      if(b->fport == th->th_sport &&
        b->faddr == ih->ip_src) {
        cb = b;
        break;
      } else {
        if(b->state == TCP_STATE_LISTEN) {
          listener = b;
        } else
          mutex_unlock(&b->mtx);
      }
    }
  }

  if(cb == NULL)
    cb = listener;

  if(cb != listener && listener != NULL)
    mutex_unlock(&listener->mtx);

  if(cb==NULL || cb->state == TCP_STATE_CLOSED){
    if(cb != NULL)
      mutex_unlock(&cb->mtx);
    mutex_unlock(&cblist_mtx);
    tcp_rx_closed(pkt, ih, th, payload_len);
    return;
  }
  mutex_unlock(&cblist_mtx);

  switch(cb->state){
  case TCP_STATE_LISTEN:
    tcp_rx_listen(pkt, ih, th, cb);
    break;
  case TCP_STATE_SYN_SENT:
    tcp_rx_synsent(pkt, ih, th, cb);
    break;
  default:
    tcp_rx_otherwise(pkt, ih, th, payload_len, cb);
    break;
  }
  mutex_unlock(&cb->mtx);

  return;
exit:
  pktbuf_free(pkt);
  return;
}

int tcp_sock_connect(void *pcb, const struct sockaddr *addr) {
  struct tcpcb *cb = (struct tcpcb *)pcb;
  int err;

  if(addr->family != PF_INET)
    return EAFNOSUPPORT;

  mutex_lock(&cb->mtx);
  switch(cb->state){
  case TCP_STATE_CLOSED:
    break;
  case TCP_STATE_LISTEN:
    queue_free_all(&cb->cbqueue, struct tcpcb, qlink, tcpcb_abort);
    break;
  default:
    err = tcpcb_get_prev_error(cb, ECONNEXIST);
    mutex_unlock(&cb->mtx);
    return err;
  }

  cb->lport = ((struct sockaddr_in *)addr)->port;
  cb->laddr = ((struct sockaddr_in *)addr)->addr;

  cb->iss = tcp_geninitseq();
  tcp_ctl_tx(cb->iss, 0, STREAM_RECV_BUF, TH_SYN, cb->faddr, cb->fport, cb->lport, cb);
  cb->snd_unack = cb->iss;
  cb->snd_nxt = cb->iss+1;
  cb->state = TCP_STATE_SYN_SENT;
  cb->opentype = ACTIVE;
  mutex_unlock(&cb->mtx);

  mutex_lock(&cblist_mtx);
  list_pushback(&cb->link, &tcpcb_list);

  mutex_unlock(&cblist_mtx);

  while(true){
    mutex_lock(&cb->mtx);
    if(cb->state == TCP_STATE_ESTABLISHED){
      mutex_unlock(&cb->mtx);
      return 0;
    }else if(cb->state == TCP_STATE_CLOSED){
      err = tcpcb_get_prev_error(cb, EAGAIN);
      mutex_unlock(&cb->mtx);
      return err;
    }else{
      mutex_unlock(&cb->mtx);
      thread_sleep(cb);
    }
  }
}

int tcp_sock_listen(void *pcb, int backlog){
  struct tcpcb *cb = (struct tcpcb *)pcb;
  int err;

  mutex_lock(&cb->mtx);
  switch(cb->state){
  case TCP_STATE_CLOSED:
  case TCP_STATE_LISTEN:
    break;
  default:
    err = tcpcb_get_prev_error(cb, ECONNEXIST);
    mutex_unlock(&cb->mtx);
    return err;
  }

  queue_free_all(&cb->cbqueue, struct tcpcb, qlink, tcpcb_abort);
  queue_init(&cb->cbqueue, backlog<=0 ? 0 : backlog);

  cb->state = TCP_STATE_LISTEN;
  cb->opentype = PASSIVE;

  mutex_unlock(&cb->mtx);

  mutex_lock(&cblist_mtx);
  list_pushback(&cb->link, &tcpcb_list);
  mutex_unlock(&cblist_mtx);

  return 0;
}

void *tcp_sock_accept(void *pcb, struct sockaddr *client_addr){
  struct tcpcb *cb = (struct tcpcb *)pcb;
  struct tcpcb *pending;
  int err;

  mutex_lock(&cb->mtx);
  switch(cb->state){
  case TCP_STATE_LISTEN:
retry:
    while(true){
      if(!queue_is_empty(&cb->cbqueue)) {
        mutex_unlock(&cb->mtx);
        pending = list_entry(queue_dequeue(&cb->cbqueue), struct tcpcb, qlink);
        mutex_lock(&pending->mtx);

        pending->iss = tcp_geninitseq();
        pending->snd_nxt = pending->iss+1;
        pending->snd_unack = pending->iss;

        tcp_ctl_tx(pending->iss, pending->rcv_nxt, STREAM_RECV_BUF, TH_SYN|TH_ACK, 
                   pending->faddr, pending->fport, cb->lport, pending);

        pending->state = TCP_STATE_SYN_RCVD;

        *(struct sockaddr_in *)client_addr = pending->foreign_addr;

        break;
      }else{
        mutex_unlock(&cb->mtx);
        //FIXME: unlock - sleep の間でwakeupされるかもしれない
        thread_sleep(cb);
        //thread_sleep_after_unlock(cb, &cb->mtx);
        mutex_lock(&cb->mtx);
      }
    }

    while(true) {
      if(pending->state == TCP_STATE_ESTABLISHED || pending->state == TCP_STATE_CLOSE_WAIT){
        mutex_unlock(&pending->mtx);
        return pending;
      }else if(pending->state == TCP_STATE_CLOSED){
        pending->is_userclosed = true;
        tcpcb_reset(pending);
        goto retry;
      }

      mutex_unlock(&pending->mtx);
      thread_sleep(pending);
      mutex_lock(&pending->mtx);
    }
    break;
  default:
    err = tcpcb_get_prev_error(cb, ENOTLISTENING);
    mutex_unlock(&cb->mtx);
    return NULL;
  }
}

int tcp_sock_send(void *pcb, const char *msg, size_t len, int flags UNUSED) {
  struct tcpcb *cb = (struct tcpcb *)pcb;
  int err;
  mutex_lock(&cb->mtx);
  switch(cb->state){
  case TCP_STATE_CLOSED:
  case TCP_STATE_LISTEN:
  case TCP_STATE_SYN_SENT:
  case TCP_STATE_SYN_RCVD:
    mutex_unlock(&cb->mtx);
    return tcpcb_get_prev_error(cb, ECONNNOTEXIST);
  case TCP_STATE_ESTABLISHED:
  case TCP_STATE_CLOSE_WAIT:
    {
      int retval = tcp_write_to_sendbuf(cb, msg, len);
      mutex_unlock(&cb->mtx);
      tcp_tx_request();
      return retval;
    }
  case TCP_STATE_FIN_WAIT_1:
  case TCP_STATE_FIN_WAIT_2:
  case TCP_STATE_CLOSING:
  case TCP_STATE_LAST_ACK:
  case TCP_STATE_TIME_WAIT:
    err = tcpcb_get_prev_error(cb, ECONNCLOSING);
    mutex_unlock(&cb->mtx);
    return err;
  default:
    return -1;
  }
}

int tcp_sock_recv(void *pcb, char *buf, size_t len, int flags UNUSED) {
  struct tcpcb *cb = (struct tcpcb *)pcb;
  int result;
  mutex_lock(&cb->mtx);
  switch(cb->state){
  case TCP_STATE_CLOSED:
  case TCP_STATE_LISTEN:
  case TCP_STATE_SYN_SENT:
  case TCP_STATE_SYN_RCVD:
    mutex_unlock(&cb->mtx);
    return tcpcb_get_prev_error(cb, ECONNNOTEXIST);
  case TCP_STATE_ESTABLISHED:
  case TCP_STATE_FIN_WAIT_1:
  case TCP_STATE_FIN_WAIT_2:
  case TCP_STATE_CLOSE_WAIT:
    result = tcp_read_from_arrival(cb, buf, len);
    mutex_unlock(&cb->mtx);
    return result;
  case TCP_STATE_CLOSING:
  case TCP_STATE_LAST_ACK:
  case TCP_STATE_TIME_WAIT:
    mutex_unlock(&cb->mtx);
    return tcpcb_get_prev_error(cb, ECONNCLOSING);
  default:
    return -1;
  }
}

int tcp_sock_close(void *pcb) {
  struct tcpcb *cb = (struct tcpcb *)pcb;
  mutex_lock(&cb->mtx);
  int result = 0;
  cb->is_userclosed = true;
  switch(cb->state){
  case TCP_STATE_CLOSED:
    tcpcb_reset(cb);
    result = ECONNNOTEXIST;
    break;
  case TCP_STATE_LISTEN:
    tcpcb_reset(cb);
    break;
  case TCP_STATE_SYN_SENT:
    tcpcb_reset(cb);
    break;
  case TCP_STATE_SYN_RCVD:
    cb->myfin_state = FIN_REQUESTED;
    cb->state = TCP_STATE_FIN_WAIT_1;
    tcp_tx_request();
    break;
  case TCP_STATE_ESTABLISHED:
    cb->myfin_state = FIN_REQUESTED;
    cb->state = TCP_STATE_FIN_WAIT_1;
    tcp_tx_request();
    break;
  case TCP_STATE_FIN_WAIT_1:
  case TCP_STATE_FIN_WAIT_2:
    result = ECONNCLOSING;
    break;
  case TCP_STATE_CLOSE_WAIT:
    cb->myfin_state = FIN_REQUESTED;
    tcpcb_timer_add(cb, TCP_FINWAIT_TIME, timinfo_new(TCP_TIMER_TYPE_FINACK));
    tcp_tx_request();
    break;
  case TCP_STATE_CLOSING:
  case TCP_STATE_LAST_ACK:
  case TCP_STATE_TIME_WAIT:
    result = ECONNCLOSING;
    break;
  }

  mutex_unlock(&cb->mtx);
  return result;
}

void *tcp_sock_init() {
  struct tcpcb *cb = malloc(sizeof(struct tcpcb));
  cb->errno = 0;
  tcpcb_init(cb);
  return cb;
}

int tcp_sock_bind(void *pcb, const struct sockaddr *addr) {
  struct tcpcb *cb = (struct tcpcb *)pcb;
  if(addr->family != PF_INET)
    return EAFNOSUPPORT;

  mutex_lock(&cblist_mtx);
  mutex_lock(&cb->mtx);
  struct sockaddr_in *inaddr = (struct sockaddr_in *)addr;
  if(inaddr->port == NEED_PORT_ALLOC)
    inaddr->port = get_unused_port();
  else if(is_used_port(inaddr->port))
    return -1;

  memcpy(&cb->local_addr, inaddr, sizeof(struct sockaddr_in));
  mutex_unlock(&cb->mtx);
  mutex_unlock(&cblist_mtx);
  return 0;
}

void timinfo_free(struct timinfo *tinfo) {
  if(tinfo->type == TCP_TIMER_TYPE_RESEND)
    pktbuf_free(tinfo->resend_pkt);
  free(tinfo);
}

void tcp_timer_do(void *arg) {
  struct timinfo *tinfo = (struct timinfo *)arg;
  switch(tinfo->type){
  case TCP_TIMER_TYPE_REMOVED:
    timinfo_free(tinfo);
    break;
  case TCP_TIMER_TYPE_FINACK:
    mutex_lock(&(tinfo->cb->mtx));
    tcpcb_reset(tinfo->cb);
    mutex_unlock(&(tinfo->cb->mtx));
    list_remove(&tinfo->link);
    timinfo_free(tinfo);
    break;
  case TCP_TIMER_TYPE_RESEND:
    mutex_lock(&(tinfo->cb->mtx));
    if(tcp_resend_from_buf(tinfo->cb, tinfo)) {
      if(tinfo->resend_is_zerownd_probe){
        //ゼロウィンドウ・プローブ(持続タイマ)
        struct tcpcb *cb = tinfo->cb;
        if(cb->snd_wnd == 0){
          tinfo->type = TCP_TIMER_TYPE_RESEND;
          tcpcb_timer_add(tinfo->cb, MIN(tinfo->msec*2, TCP_PERSIST_WAIT_MAX), tinfo);
          list_remove(&tinfo->link);
          timinfo_free(tinfo);
        }else{
          cb->snd_persisttim_enabled = false;
          list_remove(&tinfo->link);
          timinfo_free(tinfo);
        }
      }else{
        //通常の再送
        if(tinfo->msec > TCP_RESEND_WAIT_MAX){
          struct tcpcb *cb = tinfo->cb;
          tcp_ctl_tx(cb->snd_nxt, cb->rcv_nxt, cb->rcv_wnd, TH_RST, 
                    cb->faddr, cb->fport, cb->lport, NULL);
          tcpcb_reset(cb);
          list_remove(&tinfo->link);
          timinfo_free(tinfo);
        }else{
          tinfo->type = TCP_TIMER_TYPE_RESEND;
          list_remove(&tinfo->link);
          tcpcb_timer_add(tinfo->cb, tinfo->msec*2, tinfo);
        }
      }
    }
    mutex_unlock(&(tinfo->cb->mtx));
    break;
  case TCP_TIMER_TYPE_TIMEWAIT:
    mutex_lock(&tinfo->cb->mtx);
    tcpcb_reset(tinfo->cb);
    mutex_unlock(&tinfo->cb->mtx);
    list_remove(&tinfo->link);
    timinfo_free(tinfo);
    break;
  case TCP_TIMER_TYPE_DELAYACK:
    {
      struct tcpcb *cb = tinfo->cb;
      mutex_lock(&cb->mtx);
      if(cb->rcv_ack_cnt == tinfo->option.delayack.seq){
        tcp_ctl_tx(cb->snd_nxt, cb->rcv_nxt, cb->rcv_wnd, TH_ACK, 
                  cb->faddr, cb->fport, cb->lport, NULL);
      }
      mutex_unlock(&cb->mtx);
      list_remove(&tinfo->link);
      timinfo_free(tinfo);
      break;
    }
  }
}


void tcp_tx(void *arg UNUSED) {
  struct list_head *p;
  mutex_lock(&cblist_mtx);
  list_foreach(p, &tcpcb_list){
    struct tcpcb *cb = list_entry(p, struct tcpcb, link);
    mutex_lock(&cb->mtx);
    switch(cb->state){
    case TCP_STATE_ESTABLISHED:
    case TCP_STATE_CLOSE_WAIT:
    case TCP_STATE_FIN_WAIT_1:
    case TCP_STATE_CLOSING:
    case TCP_STATE_LAST_ACK:
      tcp_tx_from_buf(cb);
      thread_wakeup(cb);
      break;
    }
    mutex_unlock(&cb->mtx);
  }
  mutex_unlock(&cblist_mtx);
}

void tcp_tx_request() {
  workqueue_add(tcp_tx_wq, tcp_tx, NULL);
}
