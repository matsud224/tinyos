#include <net/ether/protohdr.h>
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

#define STREAM_SEND_BUF 1024
#define STREAM_RECV_BUF 1024

struct tcp_opt {
  u8 kind;
  u8 len;
  u8 value[0];
} PACKED;

struct tcp_opt_16 {
  u8 kind;
  u8 len;
  u16 value;
} PACKED;


static const struct tcp_opt_16 opt_mss = {
  .kind = 2,
  .len = 4,
  .value = MSS,
};
struct list_head dummy_h;
#define TCPOPT_MSS &dummy_h

#define SEQ2IDX_SEND(seq, cb) MOD((seq)-((cb)->iss+1), (cb)->send_buf_size)

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

struct tcpcb {
  struct sockaddr_in local_addr;
  struct sockaddr_in foreign_addr;

  u32 send_unack_seq; //未確認のシーケンス番号で、一番若いもの
  u32 send_next_seq; //次のデータ転送で使用できるシーケンス番号
  u16 send_window; //送信ウィンドウサイズ
  u32 send_wl1; //直近のウィンドウ更新に使用されたセグメントのシーケンス番号
  u32 send_wl2; //直近のウィンドウ更新に使用されたセグメントの確認番号
  u32 send_buf_used_len; //送信バッファに入っているデータのバイト数
  u32 send_buf_size;
  bool send_persisttim_enabled; //持続タイマが起動中か
  u32 iss; //初期送信シーケンス番号
  u8 *send_buf;

  u32 recv_next_seq; //次のデータ転送で使用できるシーケンス番号
  u16 recv_window; //受信ウィンドウサイズ
  u32 recv_buf_size; //受信バッファサイズ
  u32 recv_ack_counter; //ACKを送るごとに1づつカウントアップしていく
  u32 recv_fin_seq; //FINを受信後にセットされる、FINのシーケンス番号
  u32 irs; //初期受信シーケンス番号

  //arrival_listに連続領域が揃えばからrecv_listへ移す
  struct list_head arrival_list; //到着パケット
  struct list_head recv_list; //アプリケーションに引き渡せるパケット
  struct list_head timer_list;

  int myfin_state;
#define FIN_NOTREQUESTED 0
#define FIN_REQUESTED 1
#define FIN_SENT 2
#define FIN_ACKED 3
  u32 myfin_seq; //自身が送信したFINのシーケンス番号（ACKが来たか確かめる用に覚えとく）

  int state;
  u16 rtt;
  u16 mss; //相手との交渉で決まった値
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
      u8 flags;
      bool has_mss;
      bool is_zerownd_probe;
      struct sockaddr_in local_addr;
      struct sockaddr_in foreign_addr;
    } resend;
    struct{
      u32 seq;
    } delayack;
  } option;
#define resend_start_seq option.resend.start_seq
#define resend_end_seq option.resend.end_seq
#define resend_flags option.resend.flags
#define resend_has_mss option.resend.has_mss
#define resend_is_zerownd_probe option.resend.is_zerownd_probe
#define resend_local_addr option.resend.local_addr
#define resend_foreign_addr option.resend.foreign_addr
#define delayack_seq option.delayack.seq

  int type;
#define TCP_TIMER_TYPE_FINACK 1
#define TCP_TIMER_TYPE_RESEND 2
#define TCP_TIMER_TYPE_TIMEWAIT 3
#define TCP_TIMER_TYPE_DELAYACK 4

  struct list_head link;
  struct deferred_func *defer;
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
static u16 tcp_checksum_recv(struct ip_hdr *iphdr, struct tcp_hdr *thdr);
static u16 tcp_checksum_send(struct pktbuf *seg, in_addr_t ip_src, in_addr_t ip_dst);
static void insert_tcpopts(struct pktbuf *pkt, struct list_head *opt_list);
static u16 get_tcpopt_mss(struct tcp_hdr *thdr);
static void tcp_tx_ctrlseg(u32 seq, u32 ack, u16 win, u8 flags, struct list_head *opt_list, in_addr_t to_addr, in_port_t to_port, in_port_t my_port, bool use_resend, struct tcpcb *cb);
static void tcp_arrival_add(struct tcpcb *cb, u32 start_seq, u32 end_seq);
static u32 tcp_arrival_handover(struct tcpcb *cb);
static struct pktbuf *sendbuf_to_pktbuf(struct tcpcb *cb, u32 from_index, u32 len);
static void tcp_tx_from_buf(struct tcpcb *cb);
static bool tcp_resend_from_buf(struct tcpcb *cb, struct timerinfo *opt);
static int tcp_write_to_sendbuf(struct tcpcb *cb, const char *data, u32 len);
static int tcp_read_from_recvlist(struct tcpcb *cb, char *data, u32 len);
static void tcp_rx_closed(struct pktbuf *pkt, struct ip_hdr *iphdr, struct tcp_hdr *thdr, u16 payload_len);
static void tcp_rx_listen(struct pktbuf *pkt, struct ip_hdr *iphdr, struct tcp_hdr *thdr, struct tcpcb *cb);
static void tcp_rx_synsent(struct pktbuf *pkt, struct ip_hdr *iphdr, struct tcp_hdr *thdr, struct tcpcb *cb);
static void tcp_rx_otherwise(struct pktbuf *pkt, struct ip_hdr *iphdr, struct tcp_hdr *thdr, u16 payload_len, struct tcpcb *cb);
void tcp_rx(struct pktbuf *pkt, struct ip_hdr *iphdr);
void tcp_timer_do(void *arg);
void tcp_tx_task(void *arg UNUSED);


static void *tcp_sock_init();
static int tcp_sock_bind(void *pcb, const struct sockaddr *addr);
static int tcp_sock_close(void *pcb);
static int tcp_sock_connect(void *pcb, const struct sockaddr *addr);
static int tcp_sock_sendto(void *pcb, const u8 *msg, size_t len, int flags, struct sockaddr *dest_addr);
static size_t tcp_analyze(struct pktbuf *pkt, struct sockaddr_in *addr);
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
  //act_tsk(TCP_SEND_TASK);
  //sta_cyc(TCP_SEND_CYC);
  //act_tsk(TCP_TIMER_TASK);
  //sta_cyc(TCP_TIMER_CYC);

  socket_register_ops(PF_INET, SOCK_STREAM, &tcp_sock_ops);
}

static void tcp_timer_add(struct tcpcb *cb, u32 sec, struct timerinfo *opt) {
  //already locked.
  list_pushback(&opt->link, &cb->timer_list);
  opt->defer = defer_exec(tcp_timer_do, opt, 0, sec);
}

static void tcp_timer_remove_all(struct tcpcb *cb) {
  struct list_head *p, *tmp;
  list_foreach_safe(p, tmp, &cb->timer_list) {
    struct timerinfo *opt = list_entry(p, struct timerinfo, link);
    defer_cancel(opt->defer);
    list_remove(&opt->link);
    free(opt);
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
    tcp_tx_ctrlseg(cb->send_next_seq, 0, 0, TH_RST, NULL,
      cb->foreign_addr.addr, cb->foreign_addr.port, cb->local_addr.port, 0, NULL);
  }
  tcpcb_reset(cb);
}

void tcpcb_init(struct tcpcb *cb) {
  int errno = cb->errno;
  memset(cb, 0, sizeof(struct tcpcb));

  cb->errno = errno;

  cb->is_userclosed = false;
  cb->send_persisttim_enabled = false;

  cb->state = TCP_STATE_CLOSED;
  cb->rtt = TCP_RTT_INIT;
  cb->myfin_state = FIN_NOTREQUESTED;

  cb->recv_buf_size = STREAM_RECV_BUF;
  cb->send_buf_size = STREAM_SEND_BUF;

  cb->send_buf = NULL;
  list_init(&cb->recv_list);
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

  if(cb->send_buf!=NULL)
    free(cb->send_buf);

  list_free_all(&cb->arrival_list, struct tcp_arrival, link, tcp_arrival_free);
  list_free_all(&cb->recv_list, struct tcp_arrival, link, tcp_arrival_free);
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
  cb->send_wl1 = 0;
  cb->send_wl2 = 0;
  cb->send_buf = malloc(cb->send_buf_size);
  cb->recv_window = cb->recv_buf_size;
  return;
}

static u32 tcp_geninitseq() {
  //return systim + 64000;
  return 64000;
}

static u16 tcp_checksum_recv(struct ip_hdr *iphdr, struct tcp_hdr *thdr) {
  struct tcp_pseudo_hdr pseudo;
  pseudo.tp_src = iphdr->ip_src;
  pseudo.tp_dst = iphdr->ip_dst;
  pseudo.tp_type = 6;
  pseudo.tp_void = 0;
  pseudo.tp_len = hton16(ntoh16(iphdr->ip_len) - ip_header_len(iphdr)); //TCPヘッダ+TCPペイロードの長さ

  return checksum2((u16*)(&pseudo), (u16*)thdr, sizeof(struct tcp_pseudo_hdr), ntoh16(iphdr->ip_len) - ip_header_len(iphdr));
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

static void insert_tcpopts(struct pktbuf *pkt UNUSED, struct list_head *opt_list UNUSED){
}

//見つからければデフォルト値を返す
static u16 get_tcpopt_mss(struct tcp_hdr *thdr) {
  u8 *optptr = ((u8*)(thdr+1));
  u8 *datastart =  ((u8*)thdr) + thdr->th_off*4;
  while(optptr < datastart){
    switch(*optptr){
    case TCP_OPT_END_OF_LIST:
      return -1;
    case TCP_OPT_NOP:
      optptr++;
      break;
    case TCP_OPT_MSS:
      return ntoh16(*((u16*)(optptr+2)));
    default:
      optptr += (*(optptr+1))-2;
      break;
    }
  }
  return 536;
}


//制御用のセグメント（ペイロードなし）を送信
static void tcp_tx_ctrlseg(u32 seq, u32 ack, u16 win, u8 flags, struct list_head *opt_list, in_addr_t to_addr, in_port_t to_port, in_port_t my_port, bool use_resend, struct tcpcb *cb) {
  in_addr_t r_src, r_dst;
  struct netdev *dev = ip_routing(cb->local_addr.addr, to_addr, &r_src, &r_dst);
  if(dev == NULL)
    return; //no interface to send

  u32 optlen = get_tcpopt_len(opt_list);
  struct pktbuf *tcpseg = pktbuf_alloc(sizeof(struct ether_hdr) + sizeof(struct ip_hdr) + sizeof(struct tcp_hdr) + optlen);

  pktbuf_reserve_headroom(tcpseg, sizeof(struct ether_hdr) + sizeof(struct ip_hdr) + sizeof(struct tcp_hdr) + optlen);

  tcpopt_insert(tcpseg->head, opt_list);

  struct tcp_hdr *thdr = (struct tcp_hdr *)pktbuf_add_header(tcpseg, sizeof(struct tcp_hdr));
  thdr->th_sport = hton16(my_port);
  thdr->th_dport = hton16(to_port);
  thdr->th_seq = hton32(seq);
  thdr->th_ack = hton32(ack);
  thdr->th_flags = flags;
  thdr->th_x2 = 0;
  thdr->th_win = hton16(win);
  thdr->th_sum = 0;
  thdr->th_urp = 0;
  thdr->th_off = (sizeof(struct tcp_hdr) + optlen) / 4;
  thdr->th_sum = tcp_checksum_send(tcpseg, r_src, r_dst);

  struct timerinfo *tinfo;
  if(use_resend){
    tinfo = malloc(sizeof(struct timerinfo));
    bzero(tinfo, sizeof(struct timerinfo));
    tinfo->type = TCP_TIMER_TYPE_RESEND;
    tinfo->resend_start_seq = seq;
    tinfo->resend_end_seq = seq;
    tinfo->resend_flags = flags;
    tinfo->resend_has_mss = NULL; //TODO
    tinfo->resend_is_zerownd_probe = false;
    tinfo->resend_local_addr.port = my_port;
    tinfo->resend_foreign_addr.port = to_port;
    tinfo->resend_foreign_addr.addr = to_addr;
  }

  ip_tx(tcpseg, r_src, r_dst, IPTYPE_TCP);

  if(use_resend)
    tcp_timer_add(cb, cb->rtt*TCP_TIMER_UNIT, tinfo);
}


static void tcp_rx_closed(struct pktbuf *pkt, struct ip_hdr *iphdr, struct tcp_hdr *thdr, u16 payload_len) {
  if(!(thdr->th_flags & TH_RST)){
    if(thdr->th_flags & TH_ACK){
      tcp_tx_ctrlseg(0, ntoh32(thdr->th_seq)+payload_len, 0, TH_ACK|TH_RST, NULL, iphdr->ip_src, ntoh16(thdr->th_sport), ntoh16(thdr->th_dport), false, NULL);
    }else
      tcp_tx_ctrlseg(ntoh32(thdr->th_ack), 0, 0, TH_RST, NULL, iphdr->ip_src, ntoh16(thdr->th_sport), ntoh16(thdr->th_dport), false, NULL);
  }
  pktbuf_free(pkt);
}

static void tcp_rx_listen(struct pktbuf *pkt, struct ip_hdr *iphdr, struct tcp_hdr *thdr, struct tcpcb *cb) {
  if(thdr->th_flags & TH_RST){
    goto exit;
  }
  if(thdr->th_flags & TH_ACK){
    tcp_tx_ctrlseg(ntoh32(thdr->th_ack), 0, 0, TH_RST, NULL, iphdr->ip_src, ntoh16(thdr->th_sport), ntoh16(thdr->th_dport), false, NULL);
    goto exit;
  }
  if(thdr->th_flags & TH_SYN){
    struct tcpcb *newcb;
    if(!queue_is_full(&cb->cbqueue)) {
      newcb = tcpcb_new();
      queue_enqueue(&newcb->qlink, &cb->cbqueue);
    } else {
      goto exit;
    }

    newcb->local_addr = cb->local_addr;

    newcb->foreign_addr.addr = iphdr->ip_src;
    newcb->foreign_addr.port = ntoh16(thdr->th_sport);

    newcb->state = TCP_STATE_LISTEN;

    newcb->recv_next_seq = ntoh32(thdr->th_seq)+1;
    newcb->irs = ntoh32(thdr->th_seq);

    newcb->send_window = MIN(ntoh16(thdr->th_win), cb->send_buf_size);
    newcb->send_wl1 = ntoh32(thdr->th_seq);
    newcb->send_wl2 = ntoh32(thdr->th_ack);

    newcb->mss = MIN(get_tcpopt_mss(thdr), MSS);
    list_pushback(&cb->link, &tcpcb_list);
    task_wakeup(cb);
  }

exit:
  pktbuf_free(pkt);
  return;
}

static void tcp_rx_synsent(struct pktbuf *pkt, struct ip_hdr *iphdr, struct tcp_hdr *thdr, struct tcpcb *cb) {
  if(thdr->th_flags & TH_ACK){
    if(ntoh32(thdr->th_ack) != cb->iss+1){
      if(!(thdr->th_flags & TH_RST)){
        tcp_tx_ctrlseg(ntoh32(thdr->th_ack), 0, 0, TH_RST, NULL, iphdr->ip_src, ntoh16(thdr->th_sport), ntoh16(thdr->th_dport), false, NULL);
      }
      goto exit;
    }else{
      cb->send_unack_seq++;
    }
  }
  if(thdr->th_flags & TH_RST){
    if(LE_LE(cb->send_unack_seq, ntoh32(thdr->th_ack), cb->send_next_seq)){
      tcpcb_reset(cb);
      task_wakeup(cb);
    }
    goto exit;
  }
  if(thdr->th_flags & TH_SYN){
    cb->recv_next_seq = ntoh32(thdr->th_seq)+1;

    cb->irs = ntoh32(thdr->th_seq);
    if(thdr->th_flags & TH_ACK)
      cb->send_unack_seq = ntoh32(thdr->th_ack);
    if(cb->send_unack_seq == cb->iss){
      cb->state = TCP_STATE_SYN_RCVD;
      tcp_tx_ctrlseg(cb->iss, cb->recv_next_seq, 0, TH_SYN|TH_ACK, TCPOPT_MSS, iphdr->ip_src, ntoh16(thdr->th_sport), ntoh16(thdr->th_dport), true, cb);
      cb->recv_ack_counter++;
    }else{
      cb->send_window = MIN(ntoh16(thdr->th_win), cb->send_buf_size);
      cb->send_wl1 = ntoh32(thdr->th_seq);
      cb->send_wl2 = ntoh32(thdr->th_ack);
      cb->mss = MIN(get_tcpopt_mss(thdr), MSS);
      tcpcb_alloc_buf(cb);
      cb->state = TCP_STATE_ESTABLISHED;
      tcp_tx_ctrlseg(cb->send_next_seq, cb->recv_next_seq, cb->recv_window, TH_ACK, NULL, iphdr->ip_src, ntoh16(thdr->th_sport), ntoh16(thdr->th_dport), false, NULL);
      cb->recv_ack_counter++;
      task_wakeup(cb);
    }
  }

exit:
  pktbuf_free(pkt);
  return;
}


static void tcp_arrival_add(struct tcpcb *cb, u32 start_seq, u32 end_seq) {
  struct tcp_arrival *newarr = malloc(sizeof(struct tcp_arrival));
  newarr->start_seq = start_seq;
  newarr->end_seq = end_seq;
  list_init(&newarr->pkt_list);

  struct list_head *p, *tmp;
  list_foreach_safe(p, tmp, &cb->arrival_list){
    struct tcp_arrival *a = list_entry(p, struct tcp_arrival, link);
    bool need_remove = false;
    if(LE_LE(cb->recv_next_seq, a->start_seq-1, newarr->end_seq)) {
      //末尾が重なっているor連続している
      newarr->end_seq = a->end_seq;
      need_remove = true;
    }
    if(LE_LE(cb->recv_next_seq, newarr->start_seq, a->end_seq+1)) {
      //先頭が重なっているor連続している
      newarr->start_seq = a->start_seq;
      need_remove = true;
    }
    if(need_remove) {
      list_remove(&a->link);
      free(a);
    }
  }

  list_pushfront(&newarr->link, &cb->arrival_list);

  if(cb->recv_next_seq == start_seq){
    tcp_arrival_handover(cb);
    task_wakeup(cb);
  }
}

static u32 tcp_arrival_handover(struct tcpcb *cb) {
  struct list_head *p, *tmp;
  list_foreach_safe(p, tmp, &cb->arrival_list){
    struct tcp_arrival *a = list_entry(p, struct tcp_arrival, link);
    if(cb->recv_next_seq == a->start_seq){
      u32 len = arrival_len(a);
      cb->recv_next_seq += len;
      cb->recv_window -= len;
      list_remove(&a->link);
      list_pushback(&a->link, &cb->recv_list);
      return len;
    }
  }
  return 0;
}

static struct pktbuf *sendbuf_to_pktbuf(struct tcpcb *cb UNUSED, u32 from_index UNUSED, u32 len UNUSED) {
/*
  struct pktbuf *payload1 = struct pktbuf(false);
  struct pktbuf *payload2 = NULL;
  if(tcb->send_buf_size - from_index >= len){
    //折り返さない
    payload1->size = len;
    payload1->next = NULL;
    payload1->buf = &(tcb->send_buf[from_index]);
    return payload1;
  }else{
    //折り返す
    payload1->size = tcb->send_buf_size-from_index;
    payload1->buf = &(tcb->send_buf[from_index]);
    payload2 = new struct pktbuf(false);
    payload1->next = payload2;
    payload2->size = len - payload1->size;
    payload2->next = NULL;
    payload2->buf = tcb->send_buf;
    return payload1;
  }
*/
}


static void tcp_tx_from_buf(struct tcpcb *cb) {
  in_addr_t r_src, r_dst;
  struct netdev *dev = ip_routing(cb->local_addr.addr, cb->foreign_addr.addr, &r_src, &r_dst);
  if(dev == NULL)
    return; //no interface to send

  bool is_zerownd_probe = false;

  struct tcp_hdr tcphdr_template;
  tcphdr_template.th_dport = hton16(cb->foreign_addr.port);
  tcphdr_template.th_flags = TH_ACK;
  tcphdr_template.th_off = sizeof(struct tcp_hdr)/4;
  tcphdr_template.th_sport = hton16(cb->local_addr.port);
  tcphdr_template.th_sum = 0;
  tcphdr_template.th_urp = 0;
  tcphdr_template.th_x2 = 0;

  if(cb->send_persisttim_enabled == false && cb->send_window == 0){
    if(LE_LT(cb->send_unack_seq, cb->send_next_seq, cb->send_unack_seq+cb->send_buf_used_len)){
      //1byte以上の送信可能なデータが存在
      is_zerownd_probe = true;
      cb->send_persisttim_enabled = true;
    }
  }

  while(is_zerownd_probe ||
    (cb->send_buf_used_len>0 &&
    !LE_LT(cb->send_unack_seq,
            cb->send_unack_seq+cb->send_buf_used_len-1, cb->send_next_seq))){
    int payload_len_all;
    if(LE_LT(cb->send_next_seq, cb->send_unack_seq+cb->send_buf_used_len, cb->send_next_seq+cb->send_window))
      payload_len_all = cb->send_unack_seq+cb->send_buf_used_len - cb->send_next_seq;
    else
      payload_len_all = cb->send_next_seq+cb->send_window - cb->send_next_seq;
    int payload_len = MIN(payload_len_all, cb->mss);

    if(is_zerownd_probe) payload_len = 1;
    if(payload_len == 0) break;

    struct pktbuf *payload = sendbuf_to_pktbuf(cb, SEQ2IDX_SEND(cb->send_next_seq, cb), payload_len);

    struct timerinfo *tinfo;
    tinfo = malloc(sizeof(struct timerinfo));
    tinfo->type = TCP_TIMER_TYPE_RESEND;
    tinfo->option.resend.start_seq = cb->send_next_seq;
    tinfo->option.resend.end_seq = cb->send_next_seq + payload_len -1;
    tinfo->option.resend.flags = tcphdr_template.th_flags;
    tinfo->option.resend.has_mss = false;
    tinfo->option.resend.is_zerownd_probe = is_zerownd_probe;

    struct pktbuf *tcpseg = pktbuf_alloc();
    //memcpy(tcpseg->buf, &tcphdr_template, sizeof(struct tcp_hdr));

    struct tcp_hdr *thdr = (struct tcp_hdr*)tcpseg->head;
    thdr->th_seq = hton32(cb->send_next_seq);
    thdr->th_ack = hton32(cb->recv_next_seq);
    thdr->th_win = hton16(cb->recv_window);
    thdr->th_sum = tcp_checksum_send(tcpseg, r_src, r_dst);

    if(!is_zerownd_probe){
      cb->send_next_seq += payload_len;
      cb->send_window -= payload_len;
    }

    ip_tx(tcpseg, r_src, r_dst, IPTYPE_TCP);
    cb->recv_ack_counter++;


    tcp_timer_add(cb, cb->rtt*TCP_TIMER_UNIT, tinfo);

    is_zerownd_probe = false; //1回だけ

    //送信タスク（これ）が長時間実行されることを防ぐために、オーバランハンドラで制限をかけたほうが良さそう
  }

  if(cb->send_window > 0 && cb->myfin_state == FIN_REQUESTED){
    //送信ウィンドウが0でなく、かつここまで降りてきてFIN送信要求が出ているということは送信すべきものがもう無いのでFINを送って良い
    cb->myfin_state = FIN_SENT;
    cb->myfin_seq = cb->send_next_seq;
    cb->send_next_seq++;

    tcp_tx_ctrlseg(cb->myfin_seq, cb->recv_next_seq, cb->recv_window, TH_FIN|TH_ACK, NULL, cb->foreign_addr.addr, cb->foreign_addr.port, cb->local_addr.port, true, cb);
    cb->recv_ack_counter++;

    if(cb->state == TCP_STATE_CLOSE_WAIT)
      cb->state = TCP_STATE_LAST_ACK;
  }

  return;
}

static bool tcp_resend_from_buf(struct tcpcb *cb, struct timerinfo *opt) {
  if(!LE_LT(cb->send_unack_seq, opt->resend_end_seq, cb->send_next_seq+cb->send_window)){
    return false; //送信可能なものはない
  }

  in_addr_t r_src, r_dst;
  struct netdev *dev = ip_routing(cb->local_addr.addr, cb->foreign_addr.addr, &r_src, &r_dst);
  if(dev == NULL)
    return false; //no interface to send

  //send_ctrlsegで送ったものか（データ無し）
  if(opt->resend_start_seq == opt->resend_end_seq){
    tcp_tx_ctrlseg(opt->resend_start_seq, cb->recv_next_seq, cb->recv_window, opt->resend_flags,
             opt->resend_has_mss?TCPOPT_MSS:NULL, opt->resend_foreign_addr.addr,
             opt->resend_foreign_addr.port, opt->resend_local_addr.port, false, NULL);
    if(opt->resend_flags & TH_ACK)
      cb->recv_ack_counter++;
    return true;
  }

  int payload_len;
  if(opt->resend_is_zerownd_probe)
    payload_len = 1;
  else
    payload_len = opt->resend_end_seq - opt->resend_start_seq + 1;

  struct pktbuf *payload = sendbuf_to_pktbuf(cb, SEQ2IDX_SEND(opt->resend_start_seq, cb), payload_len);

  struct pktbuf *tcpseg = pktbuf_alloc();

  struct tcp_hdr *thdr = (struct tcp_hdr *)tcpseg->head;
  thdr->th_dport = hton16(cb->foreign_addr.port);
  thdr->th_flags = TH_ACK;
  thdr->th_off = sizeof(struct tcp_hdr)/4;
  thdr->th_sport = hton16(cb->local_addr.port);
  thdr->th_sum = 0;
  thdr->th_urp = 0;
  thdr->th_x2 = 0;
  thdr->th_seq = hton32(opt->resend_start_seq);
  thdr->th_ack = hton32(cb->recv_next_seq);
  thdr->th_win = hton16(cb->recv_window);
  thdr->th_sum = tcp_checksum_send(tcpseg, r_src, cb->foreign_addr.addr);

  ip_tx(tcpseg, r_src, r_dst, IPTYPE_TCP);
  cb->recv_ack_counter++;

  return true;
}

static int tcp_write_to_sendbuf(struct tcpcb *cb, const char *data, u32 len) {
  //already locked.
  u32 remain = len;

  while(remain > 0) {
    if(cb->send_buf_used_len < cb->send_buf_size){
      u32 write_start_index = SEQ2IDX_SEND(cb->send_unack_seq+cb->send_buf_used_len, cb);
      u32 write_len = cb->send_buf_size - write_start_index;
      memcpy(&(cb->send_buf[write_start_index]), data, write_len);
      remain -= write_len;
      cb->send_buf_used_len += write_len;
      data += write_len;
    }else{
      //already locked.
      wup_tsk(TCP_SEND_TASK);
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

static int tcp_read_from_recvlist(struct tcpcb *cb, char *data, u32 len) {
  //already locked.
  u32 remain = len;

  while(remain > 0) {
    if(list_is_empty(&cb->recv_list)) {
      switch(cb->state){
      case TCP_STATE_ESTABLISHED:
      case TCP_STATE_FIN_WAIT_1:
      case TCP_STATE_FIN_WAIT_2:
        break;
      default:
        //FINを受信していて、かつバッファが空
        if(cb->recv_fin_seq == cb->recv_next_seq){
          list_free_all(&cb->recv_list, struct tcp_arrival, link, tcp_arrival_free);
        }
        return ECONNCLOSING;
      }
    } else {
      struct tcp_arrival *a = list_entry(&cb->recv_list.head, struct tcp_arrival, link);
      u32 cplen = MIN(arrival_len(a), remain);
      memcpy(data, a->, cplen);
      pktbuf_list_remove_data(a->, cplen);
      data += cplen;
      cb->recv_window += cplen;
      remain -= cplen;
    }

    if(remain == len) {
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
    } else {
      return len - remain;
    }
  }
}


static void tcp_rx_otherwise(struct pktbuf *pkt, struct ip_hdr *iphdr, struct tcp_hdr *thdr, u16 payload_len, struct tcpcb *cb) {
  if(payload_len == 0 && cb->recv_window == 0){
    if(ntoh32(thdr->th_seq) != cb->recv_next_seq)
      goto cantrecv;
  }else if(payload_len == 0 && cb->recv_window > 0){
    if(!LE_LT(cb->recv_next_seq, ntoh32(thdr->th_seq), cb->recv_next_seq+cb->recv_window))
      goto cantrecv;
  }else if(payload_len > 0 && cb->recv_window == 0){
    goto cantrecv;
  }else if(payload_len > 0 && cb->recv_window > 0){
    if(!(LE_LT(cb->recv_next_seq, ntoh32(thdr->th_seq), cb->recv_next_seq+cb->recv_window) ||
        LE_LT(cb->recv_next_seq, ntoh32(thdr->th_seq)+payload_len-1, cb->recv_next_seq+cb->recv_window)))
      goto cantrecv;
  }

  if(thdr->th_flags & TH_RST){
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

  if(thdr->th_flags & TH_SYN){
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
      tcp_tx_ctrlseg(ntoh32(thdr->th_ack), cb->recv_next_seq, 0, TH_RST, NULL, iphdr->ip_src, ntoh16(thdr->th_sport), ntoh16(thdr->th_dport), false, NULL);
      goto exit;
      break;
    }
  }

  if(thdr->th_flags & TH_ACK){
    switch(cb->state){
    case TCP_STATE_SYN_RCVD:
      if(LE_LE(cb->send_unack_seq, hton32(thdr->th_ack), cb->send_next_seq)){
        cb->send_window = MIN(ntoh16(thdr->th_win), cb->send_buf_size);
        cb->send_wl1 = ntoh32(thdr->th_seq);
        cb->send_wl2 = ntoh32(thdr->th_ack);
        cb->send_unack_seq = hton32(thdr->th_ack);

        cb_alloc_buf(cb);

        cb->state = TCP_STATE_ESTABLISHED;
        task_wakeup(cb);
      }else{
        tcp_tx_ctrlseg(ntoh32(thdr->th_ack), cb->recv_next_seq, 0, TH_RST, NULL, iphdr->ip_src, ntoh16(thdr->th_sport), ntoh16(thdr->th_dport), false, NULL);
        goto exit;
      }
    case TCP_STATE_ESTABLISHED:
    case TCP_STATE_FIN_WAIT_1:
    case TCP_STATE_FIN_WAIT_2:
    case TCP_STATE_CLOSE_WAIT:
    case TCP_STATE_CLOSING:
      if(LT_LE(cb->send_unack_seq, ntoh32(thdr->th_ack), cb->send_next_seq)){
        u32 unack_seq_before = cb->send_unack_seq;
        cb->send_unack_seq = ntoh32(thdr->th_ack);

        if(cb->send_unack_seq >= unack_seq_before)
          cb->send_buf_used_len -= cb->send_unack_seq - unack_seq_before;
        else
          cb->send_buf_used_len = (0xffffffff - unack_seq_before + 1) + cb->send_unack_seq;
      }else if(ntoh32(thdr->th_ack) != cb->send_unack_seq){
        tcp_tx_ctrlseg(cb->send_next_seq, cb->recv_next_seq, cb->recv_window, TH_ACK, NULL, iphdr->ip_src, ntoh16(thdr->th_sport), ntoh16(thdr->th_dport), false, NULL);
        cb->recv_ack_counter++;
        goto exit;
      }
      if(LE_LE(cb->send_unack_seq, ntoh32(thdr->th_ack), cb->send_next_seq)){
        if(LT_LE(cb->send_wl1, ntoh32(thdr->th_seq), cb->recv_next_seq+cb->recv_window) ||
           (cb->send_wl1==ntoh32(thdr->th_seq)
            && LE_LE(cb->send_wl2, ntoh32(thdr->th_ack), cb->send_next_seq))){
          cb->send_window = MIN(ntoh16(thdr->th_win), cb->send_buf_size);
          cb->send_wl1 = ntoh32(thdr->th_seq);
          cb->send_wl2 = ntoh32(thdr->th_ack);
        }
      }
      if(cb->state == TCP_STATE_FIN_WAIT_1){
        if(cb->myfin_state== FIN_ACKED || (cb->myfin_state == FIN_SENT && ntoh32(thdr->th_ack)-1 == cb->myfin_seq)){
          cb->state = TCP_STATE_FIN_WAIT_2;
          cb->myfin_state = FIN_ACKED;
        }
      }
      /*if(cb->state == TCP_STATE_FIN_WAIT_2){
        if(cb->send_used_len == 0){

        }
      }*/
      if(cb->state == TCP_STATE_CLOSING){
        if(cb->myfin_state== FIN_ACKED || (cb->myfin_state == FIN_SENT && ntoh32(thdr->th_ack)-1 == cb->myfin_seq)){
          cb->state = TCP_STATE_TIME_WAIT;
          cb->myfin_state = FIN_ACKED;
        }else{
          goto exit;
        }
      }
      break;
    case TCP_STATE_LAST_ACK:
      if(cb->myfin_state== FIN_ACKED || (cb->myfin_state == FIN_SENT && ntoh32(thdr->th_ack)-1 == cb->myfin_seq)){
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
      tcp_arrival_add(cb, ((u8*)thdr)+thdr->th_off*4, payload_len, ntoh32(thdr->th_seq));
      //遅延ACKタイマ開始
      struct timerinfo *tinfo = malloc(sizeof(struct timerinfo));
      tinfo->option.delayack.seq = cb->recv_ack_counter;
      tinfo->type = TCP_TIMER_TYPE_DELAYACK;
      tcp_timer_add(cb, TCP_DELAYACK_TIME, tinfo);
      break;
    }
  }

  if(thdr->th_flags & TH_FIN){
    switch(cb->state){
    case TCP_STATE_CLOSED:
    case TCP_STATE_LISTEN:
    case TCP_STATE_SYN_SENT:
      goto exit;
      break;
    }

    /*
    if(ntoh32(thdr->th_seq) != cb->recv_next_seq)
      goto exit;
    */

    //相手からFINが送られてきたということは、こちらに全てのセグメントが到着している
    cb->recv_fin_seq = ntoh32(thdr->th_seq);
    cb->recv_next_seq = cb->recv_fin_seq+1;
    cp_send_ctrlseg(cb->send_next_seq, cb->recv_next_seq,
             cb->recv_window, TH_ACK, NULL, iphdr->ip_src, ntoh16(thdr->th_sport), ntoh16(thdr->th_dport), false, NULL);
    cb->recv_ack_counter++;

    switch(cb->state){
    case TCP_STATE_SYN_RCVD:
    case TCP_STATE_ESTABLISHED:
      cb->state = TCP_STATE_CLOSE_WAIT;
      task_wakeup(cb);
      break;
    case TCP_STATE_FIN_WAIT_1:
      if(cb->myfin_state== FIN_ACKED || (cb->myfin_state == FIN_SENT && ntoh32(thdr->th_ack)-1 == cb->myfin_seq)){
        cb->state = TCP_STATE_TIME_WAIT;
        cb->myfin_state = FIN_ACKED;
        tcp_timer_remove_all(cb);
        tcp_timer_add(cb, TCP_TIMEWAIT_TIME, TCP_TIMER_TYPE_TIMEWAIT, NULL);
      }else{
        cb->state = TCP_STATE_CLOSING;
      }
      break;
    case TCP_STATE_FIN_WAIT_2:
      cb->state = TCP_STATE_TIME_WAIT;
      tcp_timer_remove_all(cb);
      tcp_timer_add(cb, TCP_TIMEWAIT_TIME, TCP_TIMER_TYPE_TIMEWAIT, NULL);
      break;
    case TCP_STATE_TIME_WAIT:
      tcp_timer_remove_all(cb);
      tcp_timer_add(cb, TCP_TIMEWAIT_TIME, TCP_TIMER_TYPE_TIMEWAIT, NULL);
      break;
    }
  }



  goto exit;

cantrecv:
  //ACK送信
  if(!(thdr->th_flags & TH_RST)){
    tcp_tx_ctrlseg(cb->send_next_seq, cb->recv_next_seq, cb->recv_window, TH_ACK, NULL, iphdr->ip_src, ntoh16(thdr->th_sport), ntoh16(thdr->th_dport), false, NULL);
    cb->recv_ack_counter++;
  }
exit:

  pktbuf_free(pkt);
  return;
}

void tcp_rx(struct pktbuf *pkt, struct ip_hdr *iphdr) {
  struct tcp_hdr *thdr = (struct tcp_hdr *)pkt->head;

  //ブロードキャスト/マルチキャストアドレスは不許可
  if(iphdr->ip_dst != IPADDR)
    goto exit;

  //ヘッダ検査
  if(pktbuf_get_size(pkt) < sizeof(struct ether_hdr) + ip_header_len(iphdr) + sizeof(struct tcp_hdr) ||
    pktbuf_get_size(pkt) < sizeof(struct ether_hdr) + ip_header_len(iphdr) + (thdr->th_off*4)){
    goto exit;
  }

  if(tcp_checksum_recv(iphdr, thdr) != 0)
    goto exit;

  u16 payload_len = ntoh16(iphdr->ip_len) - ip_header_len(iphdr) - thdr->th_off*4;

  mutex_lock(&tcp_mtx);

  struct tcpcb *cb = NULL;
  struct list_head *p;
  list_foreach(p, &tcb_list) {
    struct tcpcb *b = list_entry(p, struct tcpcb, link);
    if(b->local_addr.port == ntoh16(thdr->th_dport)) {
      if(b->foreign_addr.port == ntoh16(thdr->th_sport) &&
        b->foreign_addr.addr == iphdr->ip_src) {
        cb = b;
        break;
      } else {
        if(b->state == TCP_STATE_LISTEN)
          cb = b;
      }
    }
  }

  if(cb==NULL || cb->state == TCP_STATE_CLOSED){
    tcp_rx_closed(pkt, iphdr, thdr, payload_len);
    mutex_unlock(&tcp_mtx);
    return;
  }

  switch(cb->state){
  case TCP_STATE_LISTEN:
    tcp_rx_listen(pkt, iphdr, thdr, cb);
    break;
  case TCP_STATE_SYN_SENT:
    tcp_rx_synsent(pkt, iphdr, thdr, cb);
    break;
  default:
    tcp_rx_otherwise(pkt, iphdr, thdr, payload_len, cb);
    break;
  }

  mutex_unlock(&tcp_mtx);

  return;
exit:
  pktbuf_free(pkt);
  return;
}

int tcp_sock_connect(void *pcb) {
  struct tcpcb *cb = (struct tcpcb *)pcb;
  switch(cb->state){
  case TCP_STATE_CLOSED:
    break;
  case TCP_STATE_LISTEN:
    queue_free_all(&cb->cbqueue, struct tcpcb, qlink, tcpcb_abort); //FIXME: cbが残ったままになる？is_userclosed=true?
    break;
  default:
    return ECONNEXIST;
  }

  cb->iss = tcp_geninitseq();
  tcp_tx_ctrlseg(cb->iss, 0, STREAM_RECV_BUF, TH_SYN, TCPOPT_MSS, cb->foreign_addr.addr, cb->foreign_addr.port, cb->local_addr.port, true, cb);
  cb->send_unack_seq = cb->iss;
  cb->send_next_seq = cb->iss+1;
  cb->state = TCP_STATE_SYN_SENT;
  cb->opentype = ACTIVE;

  cblist_add_lock(cb);
  mutex_lock(&tcp_mtx);
  while(true){
    if(cb->state == TCP_STATE_ESTABLISHED){
      mutex_unlock(&tcp_mtx);
      return 0;
    }else if(cb->state == TCP_STATE_CLOSED){
      int err = cb->errno;
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

  queue_free_all(&cb->cbqueue, struct tcpcb, qlink, tcpcbfree);
  queue_init(&cb->cbqueue, backlog<=0 ? 0 : backlog);

  cb->state = TCP_STATE_LISTEN;
  cb->opentype = PASSIVE;

  tcblist_add_lock(cb);

  return 0;
}

struct tcpcb *tcp_sock_accept(void *pcb, struct sockaddr_in *client_addr){
  struct tcpcb *cb = (struct tcpcb *)pcb;
  mutex_lock(&tcp_mtx);
  struct tcpcb *pending;

  switch(cb->state){
  case TCP_STATE_LISTEN:
retry:
    while(true){
      if(cb->cbqueue_len > 0){
        pending = cb->cbqueue[tcb->tcbqueue_head];
        tcb->tcbqueue_len--;
        tcb->tcbqueue_head = (tcb->tcbqueue_head+1)%tcb->backlog;

        pending->iss = tcp_geninitseq();
        pending->send_next_seq = pending->iss+1;
        pending->send_unack_seq = pending->iss;

        tcp_tx_ctrlseg(pending->iss, pending->recv_next_seq, STREAM_RECV_BUF, TH_SYN|TH_ACK, TCPOPT_MSS,
                   pending->foreign_addr.addr, pending->foreign_addr.port, cb->local_addr.port, true, pending);

        cb->recv_ack_counter++;
        pending->state = TCP_STATE_SYN_RCVD;

        *client_addr = pending->foreign_addr;

        //tcblist_add(pending);
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
        return pending;
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
    cb->errno = ENOTLISITENING;
    mutex_unlock(&tcp_mtx);
    return NULL;
  }
}

int tcp_sock_send(void *pcb, const char *msg, u32 len) {
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
      wup_tsk(TCP_SEND_TASK);
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
  switch(tcb->state){
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
    result = tcp_read_from_recvlist(cb, buf, len);
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
  switch(tcb->state){
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
    wup_tsk(TCP_SEND_TASK);
    break;
  case TCP_STATE_ESTABLISHED:
    cb->myfin_state = FIN_REQUESTED;
    cb->state = TCP_STATE_FIN_WAIT_1;
    wup_tsk(TCP_SEND_TASK);
    break;
  case TCP_STATE_FIN_WAIT_1:
  case TCP_STATE_FIN_WAIT_2:
    result = ECONNCLOSING;
    break;
  case TCP_STATE_CLOSE_WAIT:
    cb->myfin_state = FIN_REQUESTED;
    tcp_timer_add(tcb, TCP_FINWAIT_TIME, TCP_TIMER_TYPE_FINACK, NULL);
    wup_tsk(TCP_SEND_TASK);
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


void tcp_timer_do(void *arg) {
  struct timerinfo *tinfo = (struct timerinfo *)arg;
  mutex_lock(&tcp_mtx);

  switch(tcptimer->type){
  case TCP_TIMER_TYPE_FINACK:
    tcpcb_reset(tcptimer->cb);
    if(tcptimer->tinfoion!=NULL)
      free(tcptimer->tinfoion);
    break;
  case TCP_TIMER_TYPE_RESEND:
    if(tcp_resend_from_buf(tcptimer->cb, tcptimer->tinfoion)){
      if(tcptimer->tinfoion->tinfoion.resend.is_zerownd_probe){
        //ゼロウィンドウ・プローブ（持続タイマ）
        struct tcpcb *cb = tcptimer->cb;
        if(cb->send_window == 0){
          tcp_timer_add(tcptimer->cb, MIN(tcptimer->msec*2, TCP_PERSIST_WAIT_MAX), TCP_TIMER_TYPE_RESEND, tcptimer->tinfoion);
        }else{
          cb->send_persisttim_enabled = false;
        }
      }else{
        //通常の再送
        if(tcptimer->msec > TCP_RESEND_WAIT_MAX){
          struct tcpcb *cb = tcptimer->cb;
          tcp_tx_ctrlseg(cb->send_next_seq, cb->recv_next_seq, cb->recv_window, TH_RST, NULL,
                    cb->foreign_addr.addr, cb->foreign_addr.port, cb->local_addr.port, false, NULL);
          tcpcb_reset(cb);
        }else{
          tcp_timer_add(tcptimer->cb, tcptimer->msec*2, TCP_TIMER_TYPE_RESEND, tcptimer->option);
        }
      }
    }
    break;
  case TCP_TIMER_TYPE_TIMEWAIT:
    tcpcb_reset(tcptimer->cb);
    if(tcptimer->tinfoion!=NULL)
      free(tcptimer->tinfoion);
    break;
  case TCP_TIMER_TYPE_DELAYACK:
    {
      struct tcpcb *cb = tcptimer->cb;
      if(cb->recv_ack_counter == tcptimer->tinfoion->tinfoion.delayack.seq){
        tcp_tx_ctrlseg(cb->send_next_seq, cb->recv_next_seq, cb->recv_window, TH_ACK, NULL,
                  cb->foreign_addr.addr, cb->foreign_addr.port, cb->local_addr.port, false, NULL);
        cb->recv_ack_counter++;

      }
      if(tcptimer->tinfoion!=NULL)
        free(tcptimer->tinfoion);
      break;
    }
  }

  tcp_timer_t *temp = tcptimer;
  tcptimer = tcptimer->next;
  delete temp;

  mutex_unlock(&tcp_mtx);
}

void tcp_tx_task(void *arg UNUSED) {
  mutex_lock(&tcp_mtx);
  struct list_head *p;
  list_foreach(p, &tcpcb_list){
    struct tcpcb *cb = list_entry(p, struct tcpcb, link);
    switch(p->state){
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

