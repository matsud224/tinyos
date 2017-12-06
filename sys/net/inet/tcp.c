#include <net/inet/tcp.h>
#include <net/inet/ip.h>
#include <net/inet/protohdr.h>
#include <net/inet/util.h>
#include <net/inet/params.h>
#include <net/util.h>
#include <kern/kernlib.h>
#include <kern/lock.h>
#include <kern/task.h>
#include <net/inet/errno.h>

#define MOD(x,y) ((x) % (y))

#define tcp_header_len(th) ((th)->th_off*4)

#define STREAM_SEND_BUF 1024
#define STREAM_RECV_BUF 1024

#define SEQ2IDX_SEND(seq, cb) MOD((seq)-((cb)->iss+1), (cb)->snd_buf_len)

typedef int bool;
#define true 1
#define false 0

static mutex tcp_mtx;

struct list_head tcpcb_list;

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
  u8 *snd_buf;

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

#define TCP_OPT_END_OF_LIST 0
#define TCP_OPT_NOP 1
#define TCP_OPT_MSS 2
#define TCP_OPT_WIN_SCALE 3
#define TCP_OPT_S_ACK_PERM 4
#define TCP_OPT_S_ACK 5
#define TCP_OPT_TIMESTAMP 6


struct timerinfo {
  union{
    struct{
      u32 start_seq;
      u32 end_seq;
      u32 is_zerownd_probe;
    } resend;
    struct{
      u32 seq;
    } delayack;
  } option;
#define resend_start_seq option.resend.start_seq
#define resend_end_seq option.resend.end_seq
#define resend_is_zerownd_probe option.resend.is_zerownd_probe
#define delayack_seq option.delayack.seq
  int type;
#define TCP_TIMER_TYPE_FINACK 1
#define TCP_TIMER_TYPE_RESEND 2
#define TCP_TIMER_TYPE_TIMEWAIT 3
#define TCP_TIMER_TYPE_DELAYACK 4
  int msec;
  struct tcpcb *cb;
  struct list_head link;
  struct deferred_func *defer;
  struct pktbuf *pkt;
};

static void tcp_timer_add(struct tcpcb *cb, u32 sec, struct timerinfo *opt);
static void tcp_timer_remove_all(struct tcpcb *cb);
static bool LE_LT(u32 a, u32 b, u32 c);
static bool LT_LE(u32 a, u32 b, u32 c);
static bool LE_LE(u32 a, u32 b, u32 c);
static bool LT_LT(u32 a, u32 b, u32 c);
static void tcpcb_abort(struct tcpcb *cb);
void tcpcb_init(struct tcpcb *cb);
static void tcpcb_reset(struct tcpcb *cb);
struct tcpcb *tcpcb_new(void);
static void tcpcb_alloc_buf(struct tcpcb *cb);
static u32 tcp_geninitseq(void);
static u16 tcp_checksum_recv(struct ip_hdr *ih, struct tcp_hdr *th);
static u16 tcp_checksum_send(struct pktbuf *seg, in_addr_t ip_src, in_addr_t ip_dst);
static void tcp_ctl_tx(u32 seq, u32 ack, u16 win, u8 flags, in_addr_t to_addr, in_port_t to_port, in_port_t my_port, bool use_resend, struct tcpcb *cb);
static void tcp_arrival_add(struct pktbuf *pkt, struct tcpcb *cb, u32 start_seq, u32 end_seq);
static struct pktbuf *sendbuf_to_pktbuf(struct tcpcb *cb, u32 from_index, u32 len);
static void tcp_tx_from_buf(struct tcpcb *cb);
static bool tcp_resend_from_buf(struct tcpcb *cb, struct timerinfo *opt);
static int tcp_write_to_sendbuf(struct tcpcb *cb, const u8 *data, u32 len);
static int tcp_read_from_arrival(struct tcpcb *cb, char *data, u32 len);
static void tcp_rx_closed(struct pktbuf *pkt, struct ip_hdr *ih, struct tcp_hdr *th, u16 payload_len);
static void tcp_rx_listen(struct pktbuf *pkt, struct ip_hdr *ih, struct tcp_hdr *th, struct tcpcb *cb);
static void tcp_rx_synsent(struct pktbuf *pkt, struct ip_hdr *ih, struct tcp_hdr *th, struct tcpcb *cb);
static void tcp_rx_otherwise(struct pktbuf *pkt, struct ip_hdr *ih, struct tcp_hdr *th, u16 payload_len, struct tcpcb *cb);
void tcp_rx(struct pktbuf *pkt, struct ip_hdr *ih);
void tcp_timer_do(void *arg);
void tcp_tx_task(void *arg UNUSED);
void tcp_tx_request(void);

static void *tcp_sock_init(void);
static int tcp_sock_bind(void *pcb, const struct sockaddr *addr);
static int tcp_sock_close(void *pcb);
static int tcp_sock_connect(void *pcb, const struct sockaddr *addr);
static int tcp_sock_sendto(void *pcb, const u8 *msg, size_t len, int flags, struct sockaddr *dest_addr);
static int tcp_sock_recvfrom(void *pcb, u8 *buf, size_t len, int flags, struct sockaddr *from_addr);
static int tcp_sock_send(void *pcb, const u8 *msg, size_t len, int flags);
static int tcp_sock_recv(void *pcb, u8 *buf, size_t len, int flags);
static int tcp_sock_listen(void *pcb, int backlog);
static int tcp_sock_accept(void *pcb, struct sockaddr *client_addr);

static const struct socket_ops tcp_sock_ops = {
  .init = tcp_sock_init,
  .bind = tcp_sock_bind,
  .close = tcp_sock_close,
  .connect = tcp_sock_connect,
  .listen = tcp_sock_listen,
  .accept = tcp_sock_accept,
  .sendto = tcp_sock_sendto,
  .recvfrom = tcp_sock_recvfrom,
  .send = tcp_sock_send,
  .recv = tcp_sock_recv,
};


NET_INIT void tcp_init() {
  socket_register_ops(PF_INET, SOCK_STREAM, &tcp_sock_ops);
}

static struct timerinfo *make_timerinfo(int type) {
  struct timerinfo *ti = malloc(sizeof(struct timerinfo));
  bzero(ti, sizeof(struct timerinfo));
  ti->type = type;
  return ti;
}

static void tcp_timer_add(struct tcpcb *cb, u32 msec, struct timerinfo *tinfo) {
  //already locked.
  tinfo->cb = cb;
  tinfo->msec = msec;
  list_pushback(&tinfo->link, &cb->timer_list);
  tinfo->defer = defer_exec(tcp_timer_do, tinfo, 0, msec);
}

static void tcp_timer_remove_all(struct tcpcb *cb) {
  struct list_head *p, *tmp;
  list_foreach_safe(p, tmp, &cb->timer_list) {
    struct timerinfo *tinfo = list_entry(p, struct timerinfo, link);
    defer_cancel(tinfo->defer);
    list_remove(&tinfo->link);
    free(tinfo);
  }
}


//mod 2^32で計算
static bool LE_LT(u32 a, u32 b, u32 c) {
  if(a < c)
    return (a<=b) && (b<c);
  else if(a > c)
    return (a<=b) || (b<c);
  else
    return false;
}

static bool LT_LE(u32 a, u32 b, u32 c) {
  if(a < c)
    return (a<b) && (b<=c);
  else if(a > c)
    return (a<b) || (b<=c);
  else
    return false;
}

static bool LE_LE(u32 a, u32 b, u32 c) {
  if(a < c)
    return (a<=b) && (b<=c);
  else if(a > c)
    return (a<=b) || (b<=c);
  else
    return a==b;
}

static bool LT_LT(u32 a, u32 b, u32 c) {
  if(a < c)
    return (a<b) && (b<c);
  else if(a > c)
    return (a<b) || (b<c);
  else
    return false;
}

static void tcpcb_abort(struct tcpcb *cb) {
  //already locked.
  switch(cb->state){
  case TCP_STATE_SYN_RCVD:
  case TCP_STATE_ESTABLISHED:
  case TCP_STATE_FIN_WAIT_1:
  case TCP_STATE_FIN_WAIT_2:
  case TCP_STATE_CLOSE_WAIT:
    tcp_ctl_tx(cb->snd_nxt, 0, 0, TH_RST,
      cb->faddr, cb->fport, cb->lport, 0, NULL);
  }
  tcpcb_reset(cb);
}

void tcpcb_init(struct tcpcb *cb) {
  int errno = cb->errno;
  memset(cb, 0, sizeof(struct tcpcb));

  cb->errno = errno;

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
}

static void tcp_arrival_free(struct tcp_arrival *a) {
  list_free_all(&a->pkt_list, struct pktbuf, link, pktbuf_free);
  free(a);
}

static void tcpcb_reset(struct tcpcb *cb) {
  struct list_head *p;
  while((p = queue_dequeue(&cb->cbqueue)) != NULL) {
    struct tcpcb *b = list_entry(p, struct tcpcb, qlink);
    b->is_userclosed = true;
    tcpcb_abort(b);
  }

  if(cb->snd_buf != NULL)
    free(cb->snd_buf);

  list_free_all(&cb->arrival_list, struct tcp_arrival, link, tcp_arrival_free);
  tcp_timer_remove_all(cb);
  list_remove(&cb->link);

  if(cb->is_userclosed)
    free(cb);
  else
    tcpcb_init(cb);

  return;
}

struct tcpcb *tcpcb_new() {
  struct tcpcb *cb = malloc(sizeof(struct tcpcb));
  tcpcb_init(cb);
  return cb;
}

static void tcpcb_alloc_buf(struct tcpcb *cb) {
  cb->snd_wl1 = 0;
  cb->snd_wl2 = 0;
  cb->snd_buf = malloc(cb->snd_buf_len);
  cb->rcv_wnd = cb->rcv_buf_len;
  return;
}

static u32 tcp_geninitseq() {
  //return systim + 64000;
  return 64000;
}

static u16 tcp_checksum_recv(struct ip_hdr *ih, struct tcp_hdr *th) {
  struct tcp_pseudo_hdr pseudo;
  pseudo.tp_src = ih->ip_src;
  pseudo.tp_dst = ih->ip_dst;
  pseudo.tp_type = 6;
  pseudo.tp_void = 0;
  pseudo.tp_len = hton16(ntoh16(ih->ip_len) - ip_header_len(ih)); //TCPヘッダ+TCPペイロードの長さ

  return checksum2((u16*)(&pseudo), (u16*)th, sizeof(struct tcp_pseudo_hdr), ntoh16(ih->ip_len) - ip_header_len(ih));
}

static u16 tcp_checksum_send(struct pktbuf *seg, in_addr_t ip_src, in_addr_t ip_dst) {
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
static void tcp_ctl_tx(u32 seq, u32 ack, u16 win, u8 flags, in_addr_t to_addr, in_port_t to_port, in_port_t my_port, bool use_resend, struct tcpcb *cb) {
  in_addr_t r_src, r_dst;
  struct netdev *dev = ip_routing(cb->laddr, to_addr, &r_src, &r_dst);
  if(dev == NULL)
    return; //no interface to send

  struct pktbuf *tcpseg = pktbuf_alloc(MAX_HDRLEN_TCP);

  pktbuf_reserve_headroom(tcpseg, MAX_HDRLEN_TCP);

  struct tcp_hdr *th = (struct tcp_hdr *)pktbuf_add_header(tcpseg, sizeof(struct tcp_hdr));
  th->th_sport = hton16(my_port);
  th->th_dport = hton16(to_port);
  th->th_seq = hton32(seq);
  th->th_ack = hton32(ack);
  th->th_flags = flags;
  th->th_x2 = 0;
  th->th_win = hton16(win);
  th->th_sum = 0;
  th->th_urp = 0;
  th->th_off = (sizeof(struct tcp_hdr)) / 4;
  th->th_sum = tcp_checksum_send(tcpseg, r_src, r_dst);

  if(flags & TH_ACK)
    cb->rcv_ack_cnt++;

  struct timerinfo *tinfo;
  if(use_resend){
    tinfo = make_timerinfo(TCP_TIMER_TYPE_RESEND);
    tinfo->resend_start_seq = seq;
    tinfo->resend_end_seq = seq;
  }

  ip_tx(tcpseg, r_src, r_dst, IPTYPE_TCP);
  

  if(use_resend)
    tcp_timer_add(cb, cb->rtt*TCP_TIMER_UNIT, tinfo);
}


static void tcp_rx_closed(struct pktbuf *pkt, struct ip_hdr *ih, struct tcp_hdr *th, u16 payload_len) {
  if(!(th->th_flags & TH_RST)){
    if(th->th_flags & TH_ACK){
      tcp_ctl_tx(0, ntoh32(th->th_seq)+payload_len, 0, TH_ACK|TH_RST, ih->ip_src, ntoh16(th->th_sport), ntoh16(th->th_dport), false, NULL);
    }else
      tcp_ctl_tx(ntoh32(th->th_ack), 0, 0, TH_RST, ih->ip_src, ntoh16(th->th_sport), ntoh16(th->th_dport), false, NULL);
  }
  pktbuf_free(pkt);
}

static void tcp_rx_listen(struct pktbuf *pkt, struct ip_hdr *ih, struct tcp_hdr *th, struct tcpcb *cb) {
  if(th->th_flags & TH_RST){
    goto exit;
  }
  if(th->th_flags & TH_ACK){
    tcp_ctl_tx(ntoh32(th->th_ack), 0, 0, TH_RST, ih->ip_src, ntoh16(th->th_sport), ntoh16(th->th_dport), false, NULL);
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

    newcb->faddr = ih->ip_src;
    newcb->fport = ntoh16(th->th_sport);

    newcb->state = TCP_STATE_LISTEN;

    newcb->rcv_nxt = ntoh32(th->th_seq)+1;
    newcb->irs = ntoh32(th->th_seq);

    newcb->snd_wnd = MIN(ntoh16(th->th_win), cb->snd_buf_len);
    newcb->snd_wl1 = ntoh32(th->th_seq);
    newcb->snd_wl2 = ntoh32(th->th_ack);

    newcb->mss = MSS;
    list_pushback(&newcb->link, &tcpcb_list);
    task_wakeup(cb);
  }

exit:
  pktbuf_free(pkt);
  return;
}

static void tcp_rx_synsent(struct pktbuf *pkt, struct ip_hdr *ih, struct tcp_hdr *th, struct tcpcb *cb) {
  if(th->th_flags & TH_ACK){
    if(ntoh32(th->th_ack) != cb->iss+1){
      if(!(th->th_flags & TH_RST)){
        tcp_ctl_tx(ntoh32(th->th_ack), 0, 0, TH_RST, ih->ip_src, ntoh16(th->th_sport), ntoh16(th->th_dport), false, NULL);
      }
      goto exit;
    }else{
      cb->snd_unack++;
    }
  }
  if(th->th_flags & TH_RST){
    if(LE_LE(cb->snd_unack, ntoh32(th->th_ack), cb->snd_nxt)){
      tcpcb_reset(cb);
      task_wakeup(cb);
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
      tcp_ctl_tx(cb->iss, cb->rcv_nxt, 0, TH_SYN|TH_ACK, ih->ip_src, ntoh16(th->th_sport), ntoh16(th->th_dport), true, cb);
    }else{
      cb->snd_wnd = MIN(ntoh16(th->th_win), cb->snd_buf_len);
      cb->snd_wl1 = ntoh32(th->th_seq);
      cb->snd_wl2 = ntoh32(th->th_ack);
      cb->mss = MSS;
      tcpcb_alloc_buf(cb);
      cb->state = TCP_STATE_ESTABLISHED;
      tcp_ctl_tx(cb->snd_nxt, cb->rcv_nxt, cb->rcv_wnd, TH_ACK, ih->ip_src, ntoh16(th->th_sport), ntoh16(th->th_dport), false, NULL);
      task_wakeup(cb);
    }
  }

exit:
  pktbuf_free(pkt);
  return;
}

static struct list_head arrival_pick_head(struct tcp_arrival *a, u32 len) {
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

static u32 arrival_copy_head(struct tcp_arrival *a, char *base, u32 len) {
  u32 copied = 0;
  struct list_head pkts = arrival_pick_head(a, len);
  struct list_head *p;

  list_foreach(p, &pkts){
    struct pktbuf *pkt = list_entry(p, struct pktbuf, link);
    memcpy(base, pkt->head, pktbuf_get_size(pkt));
    copied += pktbuf_get_size(pkt);
    base += pktbuf_get_size(pkt);
  }

  list_free_all(&pkts, struct pktbuf, link, pktbuf_free);

  return copied;
}

static struct list_head arrival_pick_tail(struct tcp_arrival *a, u32 len) {
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

static void arrival_merge(struct tcp_arrival *to, struct tcp_arrival *from) {
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

static void tcp_arrival_add(struct pktbuf *pkt, struct tcpcb *cb, u32 start_seq, u32 end_seq) {
  struct tcp_arrival *newa = malloc(sizeof(struct tcp_arrival));
  newa->start_seq = start_seq;
  newa->end_seq = end_seq;
  list_init(&newa->pkt_list);
  list_pushfront(&pkt->link, &newa->pkt_list);

  struct list_head *p, *tmp;
  list_foreach_safe(p, tmp, &cb->arrival_list){
    struct tcp_arrival *a = list_entry(p, struct tcp_arrival, link);
    if(LT_LE(newa->start_seq, a->start_seq, newa->end_seq) 
         || LE_LT(newa->start_seq, a->end_seq, newa->end_seq)) {
      //重なりあり
      list_remove(p);
      arrival_merge(newa, a);
      list_free_all(&a->pkt_list, struct pktbuf, link, pktbuf_free);
      free(a);
    } else if(LT_LT(newa->end_seq, a->start_seq, a->end_seq)) {
      break;
    }
  }

  list_pushfront(&newa->link, p);

  if(cb->rcv_nxt == start_seq){
    task_wakeup(cb);
  }
}

static int tcpcb_has_arrival(struct tcpcb *cb) {
  struct tcp_arrival *a = list_entry(list_first(&cb->arrival_list), struct tcp_arrival, link);
  return (!list_is_empty(&cb->arrival_list) && a->start_seq == cb->rcv_nxt);
}

static struct pktbuf *sendbuf_to_pktbuf(struct tcpcb *cb, u32 from_index, u32 len) {
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


static void tcp_tx_from_buf(struct tcpcb *cb) {
  in_addr_t r_src, r_dst;
  struct netdev *dev = ip_routing(cb->laddr, cb->faddr, &r_src, &r_dst);
  if(dev == NULL)
    return; //no interface to send

  bool is_zerownd_probe = false;

  struct tcp_hdr th_base;
  th_base.th_dport = hton16(cb->fport);
  th_base.th_flags = TH_ACK;
  th_base.th_off = sizeof(struct tcp_hdr)/4;
  th_base.th_sport = hton16(cb->lport);
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

    struct timerinfo *tinfo = make_timerinfo(TCP_TIMER_TYPE_RESEND);
    tinfo->resend_start_seq = cb->snd_nxt;
    tinfo->resend_end_seq = cb->snd_nxt + payload_len -1;
    tinfo->resend_is_zerownd_probe = is_zerownd_probe;

    struct pktbuf *tcpseg = sendbuf_to_pktbuf(cb, SEQ2IDX_SEND(cb->snd_nxt, cb), payload_len);
    pktbuf_add_header(tcpseg, sizeof(struct tcp_hdr));
    pktbuf_copyin(tcpseg, (u8*)&th_base, sizeof(struct tcp_hdr), 0);

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

    tcp_timer_add(cb, cb->rtt*TCP_TIMER_UNIT, tinfo);

    is_zerownd_probe = false;
  }

  if(cb->snd_wnd > 0 && cb->myfin_state == FIN_REQUESTED) {
    //送信ウィンドウが0でなく、かつここまで降りてきてFIN送信要求が出ているということは送信すべきものがもう無いのでFINを送って良い
    cb->myfin_state = FIN_SENT;
    cb->myfin_seq = cb->snd_nxt;
    cb->snd_nxt++;

    tcp_ctl_tx(cb->myfin_seq, cb->rcv_nxt, cb->rcv_wnd, TH_FIN|TH_ACK, cb->faddr, cb->fport, cb->lport, true, cb);

    if(cb->state == TCP_STATE_CLOSE_WAIT)
      cb->state = TCP_STATE_LAST_ACK;
  }

  return;
}

static bool tcp_resend_from_buf(struct tcpcb *cb, struct timerinfo *tinfo) {
  if(!LE_LT(cb->snd_unack, tinfo->resend_end_seq, cb->snd_nxt+cb->snd_wnd))
    return false; //送信可能なものはない

  in_addr_t r_src, r_dst;
  struct netdev *dev = ip_routing(cb->laddr, cb->faddr, &r_src, &r_dst);
  if(dev == NULL)
    return false; //no interface to send

  struct pktbuf *pkt = tinfo->pkt;
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

static int tcp_write_to_sendbuf(struct tcpcb *cb, const u8 *data, u32 len) {
  //already locked.
  u32 remain = len;

  while(remain > 0) {
    if(cb->snd_buf_used < cb->snd_buf_len){
      u32 write_start_index = SEQ2IDX_SEND(cb->snd_unack+cb->snd_buf_used, cb);
      u32 write_len = cb->snd_buf_len - write_start_index;
      memcpy(&(cb->snd_buf[write_start_index]), data, write_len);
      remain -= write_len;
      cb->snd_buf_used += write_len;
      data += write_len;
    }else{
      //already locked.
      tcp_tx_request();
      mutex_unlock(&tcp_mtx);
      task_sleep(cb);
      mutex_lock(&tcp_mtx);
      switch(cb->state){
      case TCP_STATE_CLOSED:
      case TCP_STATE_LISTEN:
      case TCP_STATE_SYN_SENT:
      case TCP_STATE_SYN_RCVD:
        return ECONNNOTEXIST;
      case TCP_STATE_FIN_WAIT_1:
      case TCP_STATE_FIN_WAIT_2:
      case TCP_STATE_CLOSING:
      case TCP_STATE_LAST_ACK:
      case TCP_STATE_TIME_WAIT:
        return ECONNCLOSING;
      }
    }
  }
  return len - remain;
}

static int tcp_read_from_arrival(struct tcpcb *cb, char *data, u32 len) {
  //already locked.
  u32 copied = 0;

  while(copied == 0) {
    if(!tcpcb_has_arrival(cb)) {
      switch(cb->state){
      case TCP_STATE_ESTABLISHED:
      case TCP_STATE_FIN_WAIT_1:
      case TCP_STATE_FIN_WAIT_2:
        break;
      default:
        return ECONNCLOSING;
      }
    } else {
      struct tcp_arrival *a = list_entry(list_first(&cb->arrival_list), struct tcp_arrival, link);
      copied = arrival_copy_head(a, data, len);
      cb->rcv_wnd += copied;
      if(arrival_is_empty(a)) {
        list_remove(&a->link);
        tcp_arrival_free(a);
      }
      break;
    }

    //already locked.
    mutex_unlock(&tcp_mtx);
    task_sleep(cb);
    mutex_lock(&tcp_mtx);

    switch(cb->state){
    case TCP_STATE_CLOSED:
    case TCP_STATE_LISTEN:
    case TCP_STATE_SYN_SENT:
    case TCP_STATE_SYN_RCVD:
      return ECONNNOTEXIST;
    case TCP_STATE_CLOSING:
    case TCP_STATE_LAST_ACK:
    case TCP_STATE_TIME_WAIT:
      return ECONNCLOSING;
    }
  }

  return copied;
}


static void tcp_rx_otherwise(struct pktbuf *pkt, struct ip_hdr *ih, struct tcp_hdr *th, u16 payload_len, struct tcpcb *cb) {
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
        task_wakeup(cb);
      }
      goto exit;
      break;
    case TCP_STATE_ESTABLISHED:
    case TCP_STATE_FIN_WAIT_1:
    case TCP_STATE_FIN_WAIT_2:
    case TCP_STATE_CLOSE_WAIT:
      tcpcb_reset(cb);
      cb->errno = ECONNRESET;
      task_wakeup(cb);
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
      task_wakeup(cb);
      tcp_ctl_tx(ntoh32(th->th_ack), cb->rcv_nxt, 0, TH_RST, ih->ip_src, ntoh16(th->th_sport), ntoh16(th->th_dport), false, NULL);
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
        task_wakeup(cb);
      }else{
        tcp_ctl_tx(ntoh32(th->th_ack), cb->rcv_nxt, 0, TH_RST, ih->ip_src, ntoh16(th->th_sport), ntoh16(th->th_dport), false, NULL);
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
        tcp_ctl_tx(cb->snd_nxt, cb->rcv_nxt, cb->rcv_wnd, TH_ACK, ih->ip_src, ntoh16(th->th_sport), ntoh16(th->th_dport), false, NULL);
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
      tcp_arrival_add(pkt, cb, ntoh32(th->th_seq), ntoh32(th->th_seq) + payload_len);
      //遅延ACKタイマ開始
      struct timerinfo *tinfo = make_timerinfo(TCP_TIMER_TYPE_DELAYACK);
      tinfo->delayack_seq = cb->rcv_ack_cnt;
      tcp_timer_add(cb, TCP_DELAYACK_TIME, tinfo);
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
      ih->ip_src, ntoh16(th->th_sport), ntoh16(th->th_dport), false, NULL);

    switch(cb->state){
    case TCP_STATE_SYN_RCVD:
    case TCP_STATE_ESTABLISHED:
      cb->state = TCP_STATE_CLOSE_WAIT;
      task_wakeup(cb);
      break;
    case TCP_STATE_FIN_WAIT_1:
      if(cb->myfin_state== FIN_ACKED || (cb->myfin_state == FIN_SENT && ntoh32(th->th_ack)-1 == cb->myfin_seq)){
        cb->state = TCP_STATE_TIME_WAIT;
        cb->myfin_state = FIN_ACKED;
        tcp_timer_remove_all(cb);
        tcp_timer_add(cb, TCP_TIMEWAIT_TIME, make_timerinfo(TCP_TIMER_TYPE_TIMEWAIT));
      }else{
        cb->state = TCP_STATE_CLOSING;
      }
      break;
    case TCP_STATE_FIN_WAIT_2:
      cb->state = TCP_STATE_TIME_WAIT;
      tcp_timer_remove_all(cb);
      tcp_timer_add(cb, TCP_TIMEWAIT_TIME, make_timerinfo(TCP_TIMER_TYPE_TIMEWAIT));
      break;
    case TCP_STATE_TIME_WAIT:
      tcp_timer_remove_all(cb);
      tcp_timer_add(cb, TCP_TIMEWAIT_TIME, make_timerinfo(TCP_TIMER_TYPE_TIMEWAIT));
      break;
    }
  }
  goto exit;

cantrecv:
  //ACK送信
  if(!(th->th_flags & TH_RST)){
    tcp_ctl_tx(cb->snd_nxt, cb->rcv_nxt, cb->rcv_wnd, TH_ACK, ih->ip_src, ntoh16(th->th_sport), ntoh16(th->th_dport), false, NULL);
  }

exit:
  pktbuf_free(pkt);
  return;
}

void tcp_rx(struct pktbuf *pkt, struct ip_hdr *ih) {
  struct tcp_hdr *th = (struct tcp_hdr *)pkt->head;

  u32 msgsize = pktbuf_get_size(pkt);

  //ヘッダ検査
  if(msgsize < sizeof(struct tcp_hdr) ||
    msgsize < tcp_header_len(th)){
    goto exit;
  }

  if(tcp_checksum_recv(ih, th) != 0)
    goto exit;

  u16 payload_len = msgsize - tcp_header_len(th);

  mutex_lock(&tcp_mtx);

  struct tcpcb *cb = NULL;
  struct list_head *p;
  list_foreach(p, &tcpcb_list) {
    struct tcpcb *b = list_entry(p, struct tcpcb, link);
    if(b->lport == ntoh16(th->th_dport)) {
      if(b->fport == ntoh16(th->th_sport) &&
        b->faddr == ih->ip_src) {
        cb = b;
        break;
      } else {
        if(b->state == TCP_STATE_LISTEN)
          cb = b;
      }
    }
  }

  if(cb==NULL || cb->state == TCP_STATE_CLOSED){
    tcp_rx_closed(pkt, ih, th, payload_len);
    mutex_unlock(&tcp_mtx);
    return;
  }

  pktbuf_remove_header(pkt, tcp_header_len(th));
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

  mutex_unlock(&tcp_mtx);

  return;
exit:
  pktbuf_free(pkt);
  return;
}

int tcp_sock_connect(void *pcb, const struct sockaddr *addr UNUSED) {
  struct tcpcb *cb = (struct tcpcb *)pcb;
  switch(cb->state){
  case TCP_STATE_CLOSED:
    break;
  case TCP_STATE_LISTEN:
    queue_free_all(&cb->cbqueue, struct tcpcb, qlink, tcpcb_abort);
    break;
  default:
    return ECONNEXIST;
  }

  cb->iss = tcp_geninitseq();
  tcp_ctl_tx(cb->iss, 0, STREAM_RECV_BUF, TH_SYN, cb->faddr, cb->fport, cb->lport, true, cb);
  cb->snd_unack = cb->iss;
  cb->snd_nxt = cb->iss+1;
  cb->state = TCP_STATE_SYN_SENT;
  cb->opentype = ACTIVE;


  mutex_lock(&tcp_mtx);
  list_pushback(&cb->link, &tcpcb_list);
  while(true){
    if(cb->state == TCP_STATE_ESTABLISHED){
      mutex_unlock(&tcp_mtx);
      return 0;
    }else if(cb->state == TCP_STATE_CLOSED){
      mutex_unlock(&tcp_mtx);
      return EAGAIN;
    }else{
      mutex_unlock(&tcp_mtx);
      task_sleep(cb);
      mutex_lock(&tcp_mtx);
    }
  }
}

int tcp_sock_listen(void *pcb, int backlog){
  struct tcpcb *cb = (struct tcpcb *)pcb;
  switch(cb->state){
  case TCP_STATE_CLOSED:
  case TCP_STATE_LISTEN:
    break;
  default:
    return ECONNEXIST;
  }

  queue_free_all(&cb->cbqueue, struct tcpcb, qlink, tcpcb_abort);
  queue_init(&cb->cbqueue, backlog<=0 ? 0 : backlog);

  cb->state = TCP_STATE_LISTEN;
  cb->opentype = PASSIVE;

  mutex_lock(&tcp_mtx);
  list_pushback(&cb->link, &tcpcb_list);
  mutex_unlock(&tcp_mtx);

  return 0;
}

static int tcp_sock_accept(void *pcb, struct sockaddr *client_addr){
  struct tcpcb *cb = (struct tcpcb *)pcb;
  mutex_lock(&tcp_mtx);
  struct tcpcb *pending;

  switch(cb->state){
  case TCP_STATE_LISTEN:
retry:
    while(true){
      if(!queue_is_empty(&cb->cbqueue)) {
        pending = list_entry(queue_dequeue(&cb->cbqueue), struct tcpcb, qlink);

        pending->iss = tcp_geninitseq();
        pending->snd_nxt = pending->iss+1;
        pending->snd_unack = pending->iss;

        tcp_ctl_tx(pending->iss, pending->rcv_nxt, STREAM_RECV_BUF, TH_SYN|TH_ACK, 
                   pending->faddr, pending->fport, cb->lport, true, pending);

        pending->state = TCP_STATE_SYN_RCVD;

        *(struct sockaddr_in *)client_addr = pending->foreign_addr;

        break;
      }else{
        //already locked.
        mutex_unlock(&tcp_mtx);
        task_sleep(cb);
        mutex_lock(&tcp_mtx);
      }
    }

    while(true){
      if(pending->state == TCP_STATE_ESTABLISHED){
        mutex_unlock(&tcp_mtx);
        return 0;
      }else if(pending->state == TCP_STATE_CLOSED){
        pending->is_userclosed = true;
        tcpcb_reset(pending);
        goto retry;
      }

      //already locked.
      mutex_unlock(&tcp_mtx);

      task_sleep(cb);
      mutex_lock(&tcp_mtx);
    }
    break;
  default:
    mutex_unlock(&tcp_mtx);
    return ENOTLISTENING;
  }
}

int tcp_sock_send(void *pcb, const u8 *msg, size_t len, int flags UNUSED) {
  struct tcpcb *cb = (struct tcpcb *)pcb;
  mutex_lock(&tcp_mtx);
  switch(cb->state){
  case TCP_STATE_CLOSED:
  case TCP_STATE_LISTEN:
  case TCP_STATE_SYN_SENT:
  case TCP_STATE_SYN_RCVD:
    mutex_unlock(&tcp_mtx);
    return ECONNNOTEXIST;
  case TCP_STATE_ESTABLISHED:
  case TCP_STATE_CLOSE_WAIT:
    {
      int retval = tcp_write_to_sendbuf(cb, msg, len);
      mutex_unlock(&tcp_mtx);
      tcp_tx_request();
      return retval;
    }
  case TCP_STATE_FIN_WAIT_1:
  case TCP_STATE_FIN_WAIT_2:
  case TCP_STATE_CLOSING:
  case TCP_STATE_LAST_ACK:
  case TCP_STATE_TIME_WAIT:
    mutex_unlock(&tcp_mtx);
    return ECONNCLOSING;
  default:
    return -1;
  }
}

int tcp_sock_receive(void *pcb, char *buf, u32 len) {
  struct tcpcb *cb = (struct tcpcb *)pcb;
  int result;
  mutex_lock(&tcp_mtx);
  switch(cb->state){
  case TCP_STATE_CLOSED:
  case TCP_STATE_LISTEN:
  case TCP_STATE_SYN_SENT:
  case TCP_STATE_SYN_RCVD:
    mutex_unlock(&tcp_mtx);
    return ECONNNOTEXIST;
  case TCP_STATE_ESTABLISHED:
  case TCP_STATE_FIN_WAIT_1:
  case TCP_STATE_FIN_WAIT_2:
  case TCP_STATE_CLOSE_WAIT:
    result = tcp_read_from_arrival(cb, buf, len);
    mutex_unlock(&tcp_mtx);
    return result;
  case TCP_STATE_CLOSING:
  case TCP_STATE_LAST_ACK:
  case TCP_STATE_TIME_WAIT:
    mutex_unlock(&tcp_mtx);
    return ECONNCLOSING;
  default:
    return -1;
  }
}

int tcp_sock_close(void *pcb) {
  struct tcpcb *cb = (struct tcpcb *)pcb;
  mutex_lock(&tcp_mtx);
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
    tcp_timer_add(cb, TCP_FINWAIT_TIME, make_timerinfo(TCP_TIMER_TYPE_FINACK));
    tcp_tx_request();
    break;
  case TCP_STATE_CLOSING:
  case TCP_STATE_LAST_ACK:
  case TCP_STATE_TIME_WAIT:
    result = ECONNCLOSING;
    break;
  }

  mutex_unlock(&tcp_mtx);
  return result;
}

static void *tcp_sock_init() {
  return NULL;
}

static int tcp_sock_bind(void *pcb UNUSED, const struct sockaddr *addr UNUSED) {
  return -1;
}

static int tcp_sock_sendto(void *pcb UNUSED, const u8 *msg UNUSED, size_t len UNUSED, int flags UNUSED, struct sockaddr *dest_addr UNUSED) {
  return -1;
}

static int tcp_sock_recvfrom(void *pcb UNUSED, u8 *buf UNUSED, size_t len UNUSED, int flags UNUSED, struct sockaddr *from_addr UNUSED) {
  return -1;
}

static int tcp_sock_recv(void *pcb UNUSED, u8 *buf UNUSED, size_t len UNUSED, int flags UNUSED) {
  return -1;
}

void tcp_timer_do(void *arg) {
  struct timerinfo *tinfo = (struct timerinfo *)arg;
  mutex_lock(&tcp_mtx);

  switch(tinfo->type){
  case TCP_TIMER_TYPE_FINACK:
    tcpcb_reset(tinfo->cb);
    break;
  case TCP_TIMER_TYPE_RESEND:
    if(tcp_resend_from_buf(tinfo->cb, tinfo)) {
      if(tinfo->resend_is_zerownd_probe){
        //ゼロウィンドウ・プローブ（持続タイマ）
        struct tcpcb *cb = tinfo->cb;
        if(cb->snd_wnd == 0){
          tinfo->type = TCP_TIMER_TYPE_RESEND;
          tcp_timer_add(tinfo->cb, MIN(tinfo->msec*2, TCP_PERSIST_WAIT_MAX), tinfo);
        }else{
          cb->snd_persisttim_enabled = false;
        }
      }else{
        //通常の再送
        if(tinfo->msec > TCP_RESEND_WAIT_MAX){
          struct tcpcb *cb = tinfo->cb;
          tcp_ctl_tx(cb->snd_nxt, cb->rcv_nxt, cb->rcv_wnd, TH_RST, 
                    cb->faddr, cb->fport, cb->lport, false, NULL);
          tcpcb_reset(cb);
        }else{
          tinfo->type = TCP_TIMER_TYPE_RESEND;
          tcp_timer_add(tinfo->cb, tinfo->msec*2, tinfo);
        }
      }
    }
    break;
  case TCP_TIMER_TYPE_TIMEWAIT:
    tcpcb_reset(tinfo->cb);
    break;
  case TCP_TIMER_TYPE_DELAYACK:
    {
      struct tcpcb *cb = tinfo->cb;
      if(cb->rcv_ack_cnt == tinfo->option.delayack.seq){
        tcp_ctl_tx(cb->snd_nxt, cb->rcv_nxt, cb->rcv_wnd, TH_ACK, 
                  cb->faddr, cb->fport, cb->lport, false, NULL);
      }
      break;
    }
  }

  //TODO: free timerinfo struct

  mutex_unlock(&tcp_mtx);
}


void tcp_tx_task(void *arg UNUSED) {
  mutex_lock(&tcp_mtx);
  struct list_head *p;
  list_foreach(p, &tcpcb_list){
    struct tcpcb *cb = list_entry(p, struct tcpcb, link);
    switch(cb->state){
    case TCP_STATE_ESTABLISHED:
    case TCP_STATE_CLOSE_WAIT:
    case TCP_STATE_FIN_WAIT_1:
    case TCP_STATE_CLOSING:
    case TCP_STATE_LAST_ACK:
      tcp_tx_from_buf(cb);
      task_wakeup(cb);
      break;
    }
  }
  mutex_unlock(&tcp_mtx);
}

void tcp_tx_request() {

}
