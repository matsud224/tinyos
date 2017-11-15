#include <net/inet/tcp.h>
#include <net/inet/ip.h>
#include <net/inet/protohdr.h>
#include <net/inet/util.h>
#include <net/inet/params.h>
#include <net/util.h>
#include <kern/kernlib.h>
#include <kern/lock.h>

#define MOD(x,y) ((x) % (y))

#define SEQ2IDX_SEND(seq, tcb) MOD((seq)-((tcb)->iss+1), (tcb)->send_buf_size)
#define SEQ2IDX_RECV(seq, tcb) MOD((seq)-((tcb)->irs+1), (tcb)->recv_buf_size)

typedef bool int;
#define true 1
#define false 0

static mutex tcp_mtx;

struct tcp_timer_t;

struct tcp_arrival {
  struct list_head link;
  u32 start_seq;
  u32 end_seq;
};

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
  struct list_head send_list;

  u32 recv_next_seq; //次のデータ転送で使用できるシーケンス番号
  u16 recv_window; //受信ウィンドウサイズ
  u32 recv_buf_size; //受信バッファサイズ
  u32 recv_ack_counter; //ACKを送るごとに1づつカウントアップしていく
  u32 recv_fin_seq; //FINを受信後にセットされる、FINのシーケンス番号
  u32 irs; //初期受信シーケンス番号
  struct list_head recv_list;

  struct list_head arrival_list;
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


struct tcp_timer_option {
  union{
    struct{
      u32 start_seq;
      u32 end_seq;
      u8 flags;
      bool has_mss;
      bool is_zerownd_probe;
      struct  addr; //send_ctrlsegで送った場合にこちらが優先される（tcbにアドレス未登録の場合があるから）
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
#define resend_addr option.resend.addr
#define delayack_seq option.delayack.seq

  int type;
#define TCP_TIMER_TYPE_FINACK 1
#define TCP_TIMER_TYPE_RESEND 2
#define TCP_TIMER_TYPE_TIMEWAIT 3
#define TCP_TIMER_TYPE_DELAYACK 4

  struct list_head cb_link;
};

struct list_head tcpcb_list;

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
  act_tsk(TCP_SEND_TASK);
  sta_cyc(TCP_SEND_CYC);
  act_tsk(TCP_TIMER_TASK);
  sta_cyc(TCP_TIMER_CYC);

  socket_register_ops(PF_INET, SOCK_STREAM, &tcp_sock_ops);
}

static void tcp_timer_add(struct tcpcb *cb, u32 sec, struct tcp_timer_option *opt) {
  //already locked.
  list_puchback(&opt->cb_link, &cb->timer_list);
  defer_exec();
}

static void tcp_timer_remove_all(struct tcpcb *cb) {
  struct list_head *p, *tmp;
  list_foreach(p, tmp, &cb->timer_list) {
    struct tcp_timer_option *opt = list_entry(p, struct tcp_timer_option, cb_list);
    defer_cancel();
  }
}

static bool between_le_lt(u32 a, u32 b, u32 c) {
  //mod 2^32で計算
  if(a < c)
    return (a<=b) && (b<c);
  else if(a > c)
    return (a<=b) || (b<c);
  else
    return false;
}

static bool between_lt_le(u32 a, u32 b, u32 c) {
  //mod 2^32で計算
  if(a < c)
    return (a<b) && (b<=c);
  else if(a > c)
    return (a<b) || (b<=c);
  else
    return false;
}

static bool between_le_le(u32 a, u32 b, u32 c) {
  //mod 2^32で計算
  if(a < c)
    return (a<=b) && (b<=c);
  else if(a > c)
    return (a<=b) || (b<=c);
  else
    return a==b;
}

static bool between_lt_lt(u32 a, u32 b, u32 c) {
  //mod 2^32で計算
  if(a < c)
    return (a<b) && (b<c);
  else if(a > c)
    return (a<b) || (b<c);
  else
    return false;
}

static void tcp_abort(struct tcpcb *cb) {
  //already locked.
  switch(cb->state){
  case TCP_STATE_SYN_RCVD:
  case TCP_STATE_ESTABLISHED:
  case TCP_STATE_FIN_WAIT_1:
  case TCP_STATE_FIN_WAIT_2:
  case TCP_STATE_CLOSE_WAIT:
    tcp_send_ctrlseg(cb->send_next_seq, 0, 0, TH_RST, NULL,
      cb->foreign_addr.addr, cb->foreign_addr.port, cb->local_addr.port, 0, NULL);
  }
  tcpcb_reset(cb);
}

void tcpcb_init(struct tcpcb *cb) {
  int errno = cb->errno;
  memset(cb, 0, sizeof(struct tcpcb));

  //以下は、リセット後に使用される,かつ,残っていても影響が無いため復元
  cb->errno = errno;

  cb->is_userclosed = false;
  cb->send_persisttim_enabled = false;

  cb->state = TCP_STATE_CLOSED;
  cb->rtt = TCP_RTT_INIT;
  cb->myfin_state = FIN_NOTREQUESTED;

  cb->recv_buf_size = STREAM_RECV_BUF;
  cb->send_buf_size = STREAM_SEND_BUF;

  list_init(&cb->send_list);
  list_init(&cb->recv_list);
  list_init(&cb->arrival_list);
  list_init(&cb->timer_list);
}

static void tcpcb_reset(struct tcpcb *cb) {
  if(cb->backlog>0){
    int idx = cb->cbqueue_head;
    while(cb->cbqueue_len>0){
      cb->cbqueue[idx]->is_userclosed = true;
      tcp_abort(cb->cbqueue[idx]);
      idx = (idx+1) % cb->backlog;
      cb->cbqueue_len--;
    }
    delete [] cb->cbqueue;
  }
  if(list_is_empty(&cb->recvlist)){
    delete [] cb->recv_buf;
  }
  if(cb->send_buf!=NULL){
    delete [] cb->send_buf;
  }

  list_free_all(&cb->arrival_list, struct tcp_arrival, link, free);
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
  tcb_init(cb);
  return cb;
}

static void tcb_alloc_queue(struct tcpcb *cb, int backlog) {
  if(tcb->tcbqueue != NULL) {
    delete [] tcb->tcbqueue;
    tcb->tcbqueue = NULL;
  }
  if(backlog > 0){
    tcb->backlog = backlog;
    tcb->tcbqueue_head = 0;
    tcb->tcbqueue_len = 0;
    tcb->tcbqueue = new struct tcpcb*[backlog];
  }else{
    tcb->backlog = 0;
    tcb->tcbqueue_head = 0;
    tcb->tcbqueue_len = 0;
    tcb->tcbqueue = NULL;
  }
  return;
}

//ESTABLISHEDになってはじめてバッファを確保する
static void tcb_alloc_buf(struct tcpcb *tcb) {
  tcb->send_wl1 = 0;
  tcb->send_wl2 = 0;
  tcb->recv_window = tcb->recv_buf_size;
  return;
}


//初期シーケンス番号を生成する
static u32 tcp_geninitseq() {
  return systim + 64000;
}

static u16 tcp_checksum_recv(struct ip_hdr *iphdr, struct tcp_hdr *thdr) {
  struct tcp_pseudo_hdr pseudo;
  pseudo.tp_src = iphdr->ip_src;
  pseudo.tp_dst = iphdr->ip_dst;
  pseudo.tp_type = 6;
  pseudo.tp_void = 0;
  pseudo.tp_len = hton16(ntoh16(iphdr->ip_len) - ip_headerlen(iphdr)); //TCPヘッダ+TCPペイロードの長さ

  return checksum2((u16*)(&pseudo), (u16*)thdr, sizeof(struct tcp_pseudo_hdr), ntoh16(iphdr->ip_len) - ip_headerlen(iphdr));
}

static u16 tcp_checksum_send(struct pktbuf *seg, in_addr_t ip_src, in_addr_t ip_dst) {
  int segsize = struct pktbuf_totallen(seg);
  struct tcp_pseudo_hdr pseudo;
  pseudo.tp_src = ip_src;
  pseudo.tp_dst = ip_dst;
  pseudo.tp_type = 6;
  pseudo.tp_void = 0;
  pseudo.tp_len = hton16(segsize); //TCPヘッダ+TCPペイロードの長さ

  struct pktbuf *pseudo_hdr = new struct pktbuf(false);
  pseudo_hdr->buf = (char*)&pseudo;
  pseudo_hdr->size = sizeof(tcp_pseudo_hdr);
  pseudo_hdr->next = seg;
  u16 result = checksum_struct pktbuf(pseudo_hdr);
  pseudo_hdr->next = NULL;
  delete pseudo_hdr;
  return result;
}

//現時点ではMSSのみ
static struct pktbuf *make_tcpopt(bool contain_mss, u16 mss){
  int optlen = 0;
  if(contain_mss) optlen += 4;

  struct pktbuf *opt = new struct pktbuf(true);
  opt->size = optlen;
  opt->next = NULL;
  opt->buf = new char[opt->size];

  char *opt_next = opt->buf;
  if(contain_mss){
    opt_next[0] = TCP_OPT_MSS;
    opt_next[1] = 4;
    *((u16*)(opt_next+2)) = hton16(mss);
    opt_next += 4;
  }

  return opt;
}

//見つからければデフォルト値を返す
static u16 get_tcpopt_mss(tcp_hdr *thdr) {
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
static void tcp_send_ctrlseg(u32 seq, u32 ack, u16 win, u8 flags, struct pktbuf *opt,
               u8 to_addr[], u16 to_port, u16 my_port, bool use_resend, struct tcpcb *cb) {
  struct ip_hdr iphdr;
  iphdr.ip_src = IPADDR;
  iphdr.ip_dst = to_addr;

  struct tcp_hdr *thdr = new tcp_hdr;
  thdr->th_sport = hton16(my_port);
  thdr->th_dport = hton16(to_port);
  thdr->th_seq = hton32(seq);
  thdr->th_ack = hton32(ack);
  thdr->th_flags = flags;
  thdr->th_x2 = 0;
  thdr->th_win = hton16(win);
  thdr->th_sum = 0;
  thdr->th_urp = 0;

  struct pktbuf *tcpseg = new struct pktbuf(true);
  tcpseg->size = sizeof(tcp_hdr);
  tcpseg->buf = (char*)thdr;

  tcpseg->next = opt;

  thdr->th_off = (sizeof(tcp_hdr) + opt->size) / 4;
  thdr->th_sum = tcp_checksum_send(tcpseg, IPADDR, to_addr);

  tcp_timer_option *timer_opt;
  if(use_resend){
    timer_opt = new tcp_timer_option;
    timer_opt->resend_start_seq = seq;
    timer_opt->resend_end_seq = seq;
    timer_opt->resend_flags = flags;
    timer_opt->resend_has_mss = !(opt==NULL);
    timer_opt->resend_is_zerownd_probe = false;
    timer_opt->resend_addr.local_port = my_port;
    timer_opt->resend_addr.foreign_port = to_port;
    timer_opt->resend_addr.foreign_addr = to_addr;
  }

  ip_tx(tcpseg, to_addr, IPTYPE_TCP);

  if(use_resend)
    tcp_timer_add(tcb, tcb->rtt*TCP_TIMER_UNIT, TCP_TIMER_TYPE_RESEND, timer_opt);
}


static void tcp_process_closed(struct pktbuf *pkt, struct ip_hdr *iphdr, struct tcp_hdr *thdr, u16 payload_len) {
  if(!(thdr->th_flags & TH_RST)){
    if(thdr->th_flags & TH_ACK){
      tcp_send_ctrlseg(0, ntoh32(thdr->th_seq)+payload_len, 0, TH_ACK|TH_RST, NULL, iphdr->ip_src, ntoh16(thdr->th_sport), ntoh16(thdr->th_dport), false, NULL);
    }else
      tcp_send_ctrlseg(ntoh32(thdr->th_ack), 0, 0, TH_RST, NULL, iphdr->ip_src, ntoh16(thdr->th_sport), ntoh16(thdr->th_dport), false, NULL);
  }
  pktbuf_free(pkt);
}

static void tcp_process_listen(struct pktbuf *pkt, struct ip_hdr *iphdr, struct tcp_hdr *thdr, u16 payload_len, struct tcpcb *cb) {
  if(thdr->th_flags & TH_RST){
    goto exit;
  }
  if(thdr->th_flags & TH_ACK){
    tcp_send_ctrlseg(ntoh32(thdr->th_ack), 0, 0, TH_RST, NULL, iphdr->ip_src, ntoh16(thdr->th_sport), ntoh16(thdr->th_dport), false, NULL);
    goto exit;
  }
  if(thdr->th_flags & TH_SYN){
    //キューに入れる
    struct tcpcb *newtcb;
    if(tcb->backlog > tcb->tcbqueue_len){
      //空き
      newtcb = tcb_new();
      tcb->tcbqueue[(tcb->tcbqueue_head+tcb->tcbqueue_len)%tcb->backlog] = newtcb;
      tcb_setaddr_and_owner(newtcb, &(tcb->addr), tcb->ownertsk);
      tcb->tcbqueue_len++;
    }else{
      goto exit;
    }
    memcpy(newtcb->addr.partner_addr, iphdr->ip_src, IP_ADDR_LEN);

    newtcb->state = TCP_STATE_LISTEN;

    newtcb->addr.partner_port = ntoh16(thdr->th_sport);
    newtcb->recv_next_seq = ntoh32(thdr->th_seq)+1;
    newtcb->irs = ntoh32(thdr->th_seq);

    newtcb->send_window = MIN(ntoh16(thdr->th_win), tcb->send_buf_size);
    newtcb->send_wl1 = ntoh32(thdr->th_seq);
    newtcb->send_wl2 = ntoh32(thdr->th_ack);

    newtcb->mss = MIN(get_tcpopt_mss(thdr), MSS);
    list_pushback(&cb->link, &tcpcb_list);
    task_wakeup(cb);
  }

exit:
  pktbuf_free(pkt);
  return;
}

static void tcp_process_synsent(struct pktbuf *pkt, struct ip_hdr *iphdr, struct tcp_hdr *thdr, u16 payload_len, struct tcpcb *cb) {
  if(thdr->th_flags & TH_ACK){
    if(ntoh32(thdr->th_ack) != tcb->iss+1){
      if(!(thdr->th_flags & TH_RST)){
        tcp_send_ctrlseg(ntoh32(thdr->th_ack), 0, 0, TH_RST, NULL, iphdr->ip_src, ntoh16(thdr->th_sport), ntoh16(thdr->th_dport), false, NULL);
      }
      goto exit;
    }else{
      tcb->send_unack_seq++;
    }
  }
  if(thdr->th_flags & TH_RST){
    if(between_le_le(cb->send_unack_seq, ntoh32(thdr->th_ack), cb->send_next_seq)){
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
    if(cb->send_unack_seq == tcb->iss){
      cb->state = TCP_STATE_SYN_RCVD;
      tcp_send_ctrlseg(cb->iss, cb->recv_next_seq, 0, TH_SYN|TH_ACK, make_tcpopt(true, MSS), iphdr->ip_src, ntoh16(thdr->th_sport), ntoh16(thdr->th_dport), true, tcb);
      cb->recv_ack_counter++;
    }else{
      tcb->send_window = MIN(ntoh16(thdr->th_win), tcb->send_buf_size);
      tcb->send_wl1 = ntoh32(thdr->th_seq);
      tcb->send_wl2 = ntoh32(thdr->th_ack);
      tcb->mss = MIN(get_tcpopt_mss(thdr), MSS);
      tcb_alloc_buf(tcb);
      tcb->state = TCP_STATE_ESTABLISHED;
      tcp_send_ctrlseg(tcb->send_next_seq, tcb->recv_next_seq, tcb->recv_window, TH_ACK, NULL, iphdr->ip_src, ntoh16(thdr->th_sport), ntoh16(thdr->th_dport), false, NULL);
      tcb->recv_ack_counter++;
      task_wakeup(cb);
    }
  }

exit:
  pktbuf_free(pkt);
  return;
}


static void tcp_arrival_add(struct tcpcb *cb, u32 start_seq, u32 end_seq) {
  struct tcp_arrival *newarrival = malloc(sizeof(struct tcp_arrival));
  newarrival->start_seq = start_seq;
  newarrival->end_seq = end_seq;

  struct list_head *p, *tmp;
  list_foreach(p, tmp, &cb->arrival_list){
    struct tcp_arrival *a = list_entry(p, struct tcp_arrival, link);
    bool is_remove = false;
    if(between_le_le(cb->recv_next_seq, a->start_seq-1, newarrival->end_seq)) {
      //末尾が重なっているor連続している
      newarrival->end_seq = a->end_seq;
      is_remove = true;
    }
    if(between_le_le(cb->recv_next_seq, newarrival->start_seq, a->end_seq+1)) {
      //先頭が重なっているor連続している
      newarrival->start_seq = a->start_seq;
      is_remove = true;
    }
    if(is_remove) {
      list_remove(&a->link);
      free(a);
    }
  }

  list_pushfront(&newarrival->link, &cb->arrival_list);
}

//アプリケーションに引き渡せる連続したデータのバイト数を返す
static u32 tcp_arrival_handover(struct tcpcb *cb) {
  struct list_head *p, *tmp;
  list_foreach(p, tmp, &cb->arrival_list){
    struct tcp_arrival *a = list_entry(p, struct tcp_arrival, link);
    if(cb->recv_next_seq == a->start_seq){
      u32 len = a->end_seq - a->start_seq + 1;
      cb->recv_next_seq += len;
      cb->recv_window -= len;
      list_remove(&a->link);
      free(a);
      return len;
    }
  }
  return 0;
}

static void tcp_write_to_recvbuf(struct tcpcb *cb, char *data, u32 len, u32 start_seq) {
  u32 start_index = SEQ2IDX_RECV(start_seq, tcb);
  u32 end_seq;
  u32 end_index;
  if(between_le_lt(start_seq, start_seq+len-1, cb->recv_next_seq + tcb->recv_window))
    end_seq = start_seq+len-1;
  else
    end_seq = tcb->recv_next_seq+tcb->recv_window-1;

  end_index = SEQ2IDX_RECV(end_seq, cb);

  u32 end_next_index = MOD(end_index+1, tcb->recv_buf_size);
  for(u32 i=start_index; i!=end_next_index; i=MOD(i+1, tcb->recv_buf_size)){
    tcb->recv_buf[i] = *data++;
  }

  tcp_arrival_add(tcb, start_seq, end_seq);
  if(tcb->recv_next_seq == start_seq){
    tcp_arrival_handover(tcb);
    task_wakeup(cb);
  }
}

//fromインデックスからlenバイトの区間をstruct pktbufとして返す（配列が循環していることを考慮して）
static struct pktbuf *sendbuf_to_struct pktbuf(struct tcpcb *tcb, u32 from_index, u32 len) {
  struct pktbuf *payload1 = new struct pktbuf(false);
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
}


static void tcp_send_from_buf(struct tcpcb *tcb) {
  bool is_zerownd_probe = false;

  static tcp_hdr tcphdr_template;
  tcphdr_template.th_dport = hton16(tcb->addr.partner_port);
  tcphdr_template.th_flags = TH_ACK;
  tcphdr_template.th_off = sizeof(tcp_hdr)/4;
  tcphdr_template.th_sport = hton16(tcb->addr.my_port);
  tcphdr_template.th_sum = 0;
  tcphdr_template.th_urp = 0;
  tcphdr_template.th_x2 = 0;

  if(tcb->send_persisttim_enabled == false && tcb->send_window == 0){
    if(between_le_lt(tcb->send_unack_seq, tcb->send_next_seq, tcb->send_unack_seq+tcb->send_buf_used_len)){
      //1byte以上の送信可能なデータが存在
      is_zerownd_probe = true;
      tcb->send_persisttim_enabled = true;
    }
  }

  while(is_zerownd_probe ||
    (tcb->send_buf_used_len>0 &&
    !between_le_lt(tcb->send_unack_seq,
            tcb->send_unack_seq+tcb->send_buf_used_len-1, tcb->send_next_seq))){
    int payload_len_all;
    if(between_le_lt(tcb->send_next_seq, tcb->send_unack_seq+tcb->send_buf_used_len, tcb->send_next_seq+tcb->send_window))
      payload_len_all = tcb->send_unack_seq+tcb->send_buf_used_len - tcb->send_next_seq;
    else
      payload_len_all = tcb->send_next_seq+tcb->send_window - tcb->send_next_seq;
    int payload_len = MIN(payload_len_all, tcb->mss);

    if(is_zerownd_probe) payload_len = 1;
    if(payload_len == 0) break;

    struct pktbuf *payload = sendbuf_to_struct pktbuf(tcb, SEQ2IDX_SEND(tcb->send_next_seq, tcb), payload_len);

    tcp_timer_option *opt;
    opt = new tcp_timer_option;
    opt->option.resend.start_seq = tcb->send_next_seq;
    opt->option.resend.end_seq = tcb->send_next_seq + payload_len -1;
    opt->option.resend.flags = tcphdr_template.th_flags;
    opt->option.resend.has_mss = false;
    opt->option.resend.is_zerownd_probe = is_zerownd_probe;

    struct pktbuf *tcpseg = new struct pktbuf(true);
    tcpseg->size = sizeof(tcp_hdr);
    tcpseg->buf = new char[sizeof(tcp_hdr)];
    memcpy(tcpseg->buf, &tcphdr_template, sizeof(tcp_hdr));
    tcpseg->next = payload;

    tcp_hdr *thdr = (tcp_hdr*)tcpseg->buf;
    thdr->th_seq = hton32(tcb->send_next_seq);
    thdr->th_ack = hton32(tcb->recv_next_seq);
    thdr->th_win = hton16(tcb->recv_window);
    thdr->th_sum = tcp_checksum_send(tcpseg, IPADDR, tcb->addr.partner_addr);

    if(!is_zerownd_probe){
      tcb->send_next_seq += payload_len;
      tcb->send_window -= payload_len;
    }

    ip_send(tcpseg, tcb->addr.partner_addr, IPTYPE_TCP);
    tcb->recv_ack_counter++;


    tcp_timer_add(tcb, tcb->rtt*TCP_TIMER_UNIT, TCP_TIMER_TYPE_RESEND, opt);

    is_zerownd_probe = false; //1回だけ

    //送信タスク（これ）が長時間実行されることを防ぐために、オーバランハンドラで制限をかけたほうが良さそう
  }

  if(tcb->send_window > 0 && tcb->myfin_state == FIN_REQUESTED){
    //送信ウィンドウが0でなく、かつここまで降りてきてFIN送信要求が出ているということは送信すべきものがもう無いのでFINを送って良い
    tcb->myfin_state = FIN_SENT;
    tcb->myfin_seq = tcb->send_next_seq;
    tcb->send_next_seq++;

    tcp_send_ctrlseg(tcb->myfin_seq, tcb->recv_next_seq, tcb->recv_window, TH_FIN|TH_ACK, NULL, tcb->addr.partner_addr, tcb->addr.partner_port, tcb->addr.my_port, true, tcb);
    tcb->recv_ack_counter++;

    if(tcb->state == TCP_STATE_CLOSE_WAIT)
      tcb->state = TCP_STATE_LAST_ACK;
  }

  return;
}

static bool tcp_resend_from_buf(struct tcpcb *tcb, tcp_timer_option *opt) {
  if(!between_le_lt(tcb->send_unack_seq, opt->resend_end_seq, tcb->send_next_seq+tcb->send_window)){
    return false; //送信可能なものはない
  }

  //send_ctrlsegで送ったものか（データ無し）
  if(opt->resend_start_seq == opt->resend_end_seq){
    tcp_send_ctrlseg(opt->resend_start_seq, tcb->recv_next_seq, tcb->recv_window, opt->resend_flags,
             opt->resend_has_mss?make_tcpopt(true, MSS):NULL, opt->resend_addr.partner_addr,
             opt->resend_addr.partner_port, opt->resend_addr.my_port, false, NULL);
    if(opt->resend_flags & TH_ACK)
      tcb->recv_ack_counter++;
    return true;
  }

  int payload_len;
  if(opt->resend_is_zerownd_probe)
    payload_len = 1;
  else
    payload_len = opt->resend_end_seq - opt->resend_start_seq + 1;

  struct pktbuf *payload = sendbuf_to_struct pktbuf(tcb, SEQ2IDX_SEND(opt->resend_start_seq, tcb), payload_len);

  struct pktbuf *tcpseg = new struct pktbuf(true);
  tcpseg->size = sizeof(tcp_hdr);
  tcpseg->buf = new char[sizeof(tcp_hdr)];
  tcpseg->next = payload;

  tcp_hdr *thdr = (tcp_hdr*)tcpseg->buf;
  thdr->th_dport = hton16(tcb->addr.partner_port);
  thdr->th_flags = TH_ACK;
  thdr->th_off = sizeof(tcp_hdr)/4;
  thdr->th_sport = hton16(tcb->addr.my_port);
  thdr->th_sum = 0;
  thdr->th_urp = 0;
  thdr->th_x2 = 0;
  thdr->th_seq = hton32(opt->resend_start_seq);
  thdr->th_ack = hton32(tcb->recv_next_seq);
  thdr->th_win = hton16(tcb->recv_window);
  thdr->th_sum = tcp_checksum_send(tcpseg, IPADDR, tcb->addr.partner_addr);

  ip_send(tcpseg, tcb->addr.partner_addr, IPTYPE_TCP);
  tcb->recv_ack_counter++;

  return true;
}

static int tcp_write_to_sendbuf(struct tcpcb *tcb, const char *data, u32 len) {
  //already locked.
  u32 remain = len;

  while(remain > 0){
    if(tcb->send_buf_used_len < tcb->send_buf_size){
      tcb->send_buf[SEQ2IDX_SEND(tcb->send_unack_seq+tcb->send_buf_used_len, tcb)] = *data++;
      remain--;
      tcb->send_buf_used_len++;
    }else{
      //already locked.
      wup_tsk(TCP_SEND_TASK);
      mutex_unlock(&tcp_mtx);
      if(tslp_tsk(timeout) == E_TMOUT){
        mutex_lock(&tcp_mtx);
        //ロックされている状態でtcp_sendにreturn
        return len-remain;
      }
      mutex_lock(&tcp_mtx);
      switch(tcb->state){
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

static int tcp_read_from_recvbuf(struct tcpcb *tcb, char *data, u32 len) {
  //already locked.
  u32 remain = len;
  u32 head_index = SEQ2IDX_RECV(tcb->recv_next_seq-(tcb->recv_buf_size-tcb->recv_window), tcb);
  while(true){
    if(tcb->recv_window == tcb->recv_buf_size){
      //バッファが空
      switch(tcb->state){
      case TCP_STATE_ESTABLISHED:
      case TCP_STATE_FIN_WAIT_1:
      case TCP_STATE_FIN_WAIT_2:
        break;
      default:
        //FINを受信していて、かつバッファが空
        if(tcb->recv_fin_seq==tcb->recv_next_seq){
          if(tcb->recv_buf!=NULL){
            delete [] tcb->recv_buf;
            tcb->recv_buf = NULL;
          }
        }
        return ECONNCLOSING;
      }
    }
    while(tcb->recv_window != tcb->recv_buf_size && remain > 0){
      *data++ = tcb->recv_buf[head_index];
      tcb->recv_window++;
      remain--;
      head_index = MOD(head_index+1, tcb->recv_buf_size);
    }
    //最低1byteは読む
    if(remain == len){
      //already locked.
      mutex_unlock(&tcp_mtx);
      if(tslp_tsk(timeout) == E_TMOUT){
        mutex_lock(&tcp_mtx);
        //ロック状態でreturn
        return ETIMEOUT;
      }
      mutex_lock(&tcp_mtx);

      switch(tcb->state){
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
    }else{
      return len - remain;
    }
  }
}


static void tcp_process_otherwise(ether_flame *flm, ip_hdr *iphdr, tcp_hdr *thdr, u16 payload_len, struct tcpcb *tcb) {

  if(payload_len == 0 && tcb->recv_window == 0){
    if(ntoh32(thdr->th_seq) != tcb->recv_next_seq)
      goto cantrecv;
  }else if(payload_len == 0 && tcb->recv_window > 0){
    if(!between_le_lt(tcb->recv_next_seq, ntoh32(thdr->th_seq), tcb->recv_next_seq+tcb->recv_window))
      goto cantrecv;
  }else if(payload_len > 0 && tcb->recv_window == 0){
    goto cantrecv;
  }else if(payload_len > 0 && tcb->recv_window > 0){
    if(!(between_le_lt(tcb->recv_next_seq, ntoh32(thdr->th_seq), tcb->recv_next_seq+tcb->recv_window) ||
        between_le_lt(tcb->recv_next_seq, ntoh32(thdr->th_seq)+payload_len-1, tcb->recv_next_seq+tcb->recv_window)))
      goto cantrecv;
  }

  if(thdr->th_flags & TH_RST){
    switch(tcb->state){
    case TCP_STATE_SYN_RCVD:
      if(tcb->opentype == PASSIVE){
        tcb->state = TCP_STATE_LISTEN;
      }else{
        tcb_reset(tcb);
        tcb->errno = ECONNREFUSED;
        task_wakeup(cb);
      }
      goto exit;
      break;
    case TCP_STATE_ESTABLISHED:
    case TCP_STATE_FIN_WAIT_1:
    case TCP_STATE_FIN_WAIT_2:
    case TCP_STATE_CLOSE_WAIT:
      tcb_reset(tcb);
      tcb->errno = ECONNRESET;
      task_wakeup(cb);
      goto exit;
      break;
    case TCP_STATE_CLOSING:
    case TCP_STATE_LAST_ACK:
    case TCP_STATE_TIME_WAIT:
      tcb_reset(tcb);
      goto exit;
      break;
    }
  }

  if(thdr->th_flags & TH_SYN){
    switch(tcb->state){
    case TCP_STATE_SYN_RCVD:
      if(tcb->opentype == PASSIVE){
        tcb->state = TCP_STATE_LISTEN;
        goto exit;
      }
    case TCP_STATE_ESTABLISHED:
    case TCP_STATE_FIN_WAIT_1:
    case TCP_STATE_FIN_WAIT_2:
    case TCP_STATE_CLOSE_WAIT:
    case TCP_STATE_CLOSING:
    case TCP_STATE_LAST_ACK:
    case TCP_STATE_TIME_WAIT:
      tcb_reset(tcb);
      tcb->errno = ECONNRESET;
      task_wakeup(cb);
      tcp_send_ctrlseg(ntoh32(thdr->th_ack), tcb->recv_next_seq, 0, TH_RST, NULL, iphdr->ip_src, ntoh16(thdr->th_sport), ntoh16(thdr->th_dport), false, NULL);
      goto exit;
      break;
    }
  }

  if(thdr->th_flags & TH_ACK){
    switch(tcb->state){
    case TCP_STATE_SYN_RCVD:
      if(between_le_le(tcb->send_unack_seq, hton32(thdr->th_ack), tcb->send_next_seq)){
        tcb->send_window = MIN(ntoh16(thdr->th_win), tcb->send_buf_size);
        tcb->send_wl1 = ntoh32(thdr->th_seq);
        tcb->send_wl2 = ntoh32(thdr->th_ack);
        tcb->send_unack_seq = hton32(thdr->th_ack);

        tcb_alloc_buf(tcb);

        tcb->state = TCP_STATE_ESTABLISHED;
        task_wakeup(cb);
      }else{
        tcp_send_ctrlseg(ntoh32(thdr->th_ack), tcb->recv_next_seq, 0, TH_RST, NULL, iphdr->ip_src, ntoh16(thdr->th_sport), ntoh16(thdr->th_dport), false, NULL);
        goto exit;
      }
    case TCP_STATE_ESTABLISHED:
    case TCP_STATE_FIN_WAIT_1:
    case TCP_STATE_FIN_WAIT_2:
    case TCP_STATE_CLOSE_WAIT:
    case TCP_STATE_CLOSING:
      if(between_lt_le(tcb->send_unack_seq, ntoh32(thdr->th_ack), tcb->send_next_seq)){
        u32 unack_seq_before = tcb->send_unack_seq;
        tcb->send_unack_seq = ntoh32(thdr->th_ack);

        if(tcb->send_unack_seq >= unack_seq_before)
          tcb->send_buf_used_len -= tcb->send_unack_seq - unack_seq_before;
        else
          tcb->send_buf_used_len = (0xffffffff - unack_seq_before + 1) + tcb->send_unack_seq;
      }else if(ntoh32(thdr->th_ack) != tcb->send_unack_seq){
        tcp_send_ctrlseg(tcb->send_next_seq, tcb->recv_next_seq, tcb->recv_window, TH_ACK, NULL, iphdr->ip_src, ntoh16(thdr->th_sport), ntoh16(thdr->th_dport), false, NULL);
        tcb->recv_ack_counter++;
        goto exit;
      }
      if(between_le_le(tcb->send_unack_seq, ntoh32(thdr->th_ack), tcb->send_next_seq)){
        if(between_lt_le(tcb->send_wl1, ntoh32(thdr->th_seq), tcb->recv_next_seq+tcb->recv_window) ||
           (tcb->send_wl1==ntoh32(thdr->th_seq)
            && between_le_le(tcb->send_wl2, ntoh32(thdr->th_ack), tcb->send_next_seq))){
          tcb->send_window = MIN(ntoh16(thdr->th_win), tcb->send_buf_size);
          tcb->send_wl1 = ntoh32(thdr->th_seq);
          tcb->send_wl2 = ntoh32(thdr->th_ack);
        }
      }
      if(tcb->state == TCP_STATE_FIN_WAIT_1){
        if(tcb->myfin_state== FIN_ACKED || (tcb->myfin_state == FIN_SENT && ntoh32(thdr->th_ack)-1 == tcb->myfin_seq)){
          tcb->state = TCP_STATE_FIN_WAIT_2;
          tcb->myfin_state = FIN_ACKED;
        }
      }
      /*if(tcb->state == TCP_STATE_FIN_WAIT_2){
        if(tcb->send_used_len == 0){

        }
      }*/
      if(tcb->state == TCP_STATE_CLOSING){
        if(tcb->myfin_state== FIN_ACKED || (tcb->myfin_state == FIN_SENT && ntoh32(thdr->th_ack)-1 == tcb->myfin_seq)){
          tcb->state = TCP_STATE_TIME_WAIT;
          tcb->myfin_state = FIN_ACKED;
        }else{
          goto exit;
        }
      }
      break;
    case TCP_STATE_LAST_ACK:
      if(tcb->myfin_state== FIN_ACKED || (tcb->myfin_state == FIN_SENT && ntoh32(thdr->th_ack)-1 == tcb->myfin_seq)){
        tcb_reset(tcb);
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
    switch(tcb->state){
    case TCP_STATE_ESTABLISHED:
    case TCP_STATE_FIN_WAIT_1:
    case TCP_STATE_FIN_WAIT_2:
      tcp_write_to_recvbuf(tcb, ((char*)thdr)+thdr->th_off*4, payload_len, ntoh32(thdr->th_seq));
      //遅延ACKタイマ開始
      struct tcp_timer_option *opt = malloc(sizeof(struct tcp_timer_option));
      opt->option.delayack.seq = cb->recv_ack_counter;
      tcp_timer_add(cb, TCP_DELAYACK_TIME, TCP_TIMER_TYPE_DELAYACK, opt);
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
    if(ntoh32(thdr->th_seq) != tcb->recv_next_seq)
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
    tcp_send_ctrlseg(cb->send_next_seq, cb->recv_next_seq, cb->recv_window, TH_ACK, NULL, iphdr->ip_src, ntoh16(thdr->th_sport), ntoh16(thdr->th_dport), false, NULL);
    cb->recv_ack_counter++;
  }
exit:

  pktbuf_free(pkt);
  return;
}

void tcp_process(struct pktbuf *pkt, struct ip_hdr *iphdr) {
  struct tcp_hdr *thdr = pkt->head;

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
    tcp_process_closed(pkt, iphdr, thdr, payload_len);
    mutex_unlock(&tcp_mtx);
    return;
  }

  switch(cb->state){
  case TCP_STATE_LISTEN:
    tcp_process_listen(pkt, iphdr, thdr, payload_len, cb);
    break;
  case TCP_STATE_SYN_SENT:
    tcp_process_synsent(pkt, iphdr, thdr, payload_len, cb);
    break;
  default:
    tcp_process_otherwise(pkt, iphdr, thdr, payload_len, cb);
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
    cb->backlog = 0;
    if(cb->cbqueue != NULL){
      delete [] cb->cbqueue;
      cb->cbqueue = NULL;
    }
    break;
  default:
    return ECONNEXIST;
  }

  cb->iss = tcp_geninitseq();
  tcp_send_ctrlseg(cb->iss, 0, STREAM_RECV_BUF, TH_SYN, make_tcpopt(true, MSS), cb->foreign_addr.addr, cb->foreign_addr.port, cb->local_addr.port, true, cb);
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
  tcpcb_alloc_queue(cb, backlog);
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

        tcp_send_ctrlseg(pending->iss, pending->recv_next_seq, STREAM_RECV_BUF, TH_SYN|TH_ACK, make_tcpopt(true, MSS),
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
    result = tcp_read_from_recvbuf(cb, buf, len);
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
  tcb->is_userclosed = true;
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
    tcb->myfin_state = FIN_REQUESTED;
    tcb->state = TCP_STATE_FIN_WAIT_1;
    wup_tsk(TCP_SEND_TASK);
    break;
  case TCP_STATE_ESTABLISHED:
    tcb->myfin_state = FIN_REQUESTED;
    tcb->state = TCP_STATE_FIN_WAIT_1;
    wup_tsk(TCP_SEND_TASK);
    break;
  case TCP_STATE_FIN_WAIT_1:
  case TCP_STATE_FIN_WAIT_2:
    result = ECONNCLOSING;
    break;
  case TCP_STATE_CLOSE_WAIT:
    tcb->myfin_state = FIN_REQUESTED;
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
  struct tcp_timer_option *opt = (struct tcp_timer_option *)arg;
  mutex_lock(&tcp_mtx);

  switch(tcptimer->type){
  case TCP_TIMER_TYPE_FINACK:
    tcb_reset(tcptimer->tcb);
    if(tcptimer->option!=NULL)
      delete tcptimer->option;
    break;
  case TCP_TIMER_TYPE_RESEND:
    if(tcp_resend_from_buf(tcptimer->tcb, tcptimer->option)){
      if(tcptimer->option->option.resend.is_zerownd_probe){
        //ゼロウィンドウ・プローブ（持続タイマ）
        struct tcpcb *tcb = tcptimer->cb;
        if(tcb->send_window == 0){
          tcp_timer_add(tcptimer->tcb, MIN(tcptimer->msec*2, TCP_PERSIST_WAIT_MAX), TCP_TIMER_TYPE_RESEND, tcptimer->option);
        }else{
          tcb->send_persisttim_enabled = false;
        }
      }else{
        //通常の再送
        if(tcptimer->msec > TCP_RESEND_WAIT_MAX){
          struct tcpcb *tcb = tcptimer->tcb;
          tcp_send_ctrlseg(tcb->send_next_seq, tcb->recv_next_seq, tcb->recv_window, TH_RST, NULL,
                    tcb->addr.partner_addr, tcb->addr.partner_port, tcb->addr.my_port, false, NULL);
          tcb_reset(tcb);
        }else{
          tcp_timer_add(tcptimer->tcb, tcptimer->msec*2, TCP_TIMER_TYPE_RESEND, tcptimer->option);
        }
      }
    }
    break;
  case TCP_TIMER_TYPE_TIMEWAIT:
    tcb_reset(tcptimer->tcb);
    if(tcptimer->option!=NULL)
      delete tcptimer->option;
    break;
  case TCP_TIMER_TYPE_DELAYACK:
    {
      struct tcpcb *tcb = tcptimer->tcb;
      if(tcb->recv_ack_counter == tcptimer->option->option.delayack.seq){
        tcp_send_ctrlseg(tcb->send_next_seq, tcb->recv_next_seq, tcb->recv_window, TH_ACK, NULL,
                  tcb->addr.partner_addr, tcb->addr.partner_port, tcb->addr.my_port, false, NULL);
        tcb->recv_ack_counter++;
      }
      if(tcptimer->option!=NULL)
        delete tcptimer->option;
      break;
    }
  }

  tcp_timer_t *temp = tcptimer;
  tcptimer = tcptimer->next;
  delete temp;

  mutex_unlock(&tcp_mtx);
}

void tcp_send_task(void *arg UNUSED) {
  while(1) {
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
        tcp_send_from_buf(cb);
        task_wakeup(cb);
        break;
      }
    }

    mutex_unlock(&tcp_mtx);
    slp_tsk();
  }
}

