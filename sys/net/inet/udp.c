#include <net/inet/udp.h>
#include <net/inet/protohdr.h>
#include <net/inet/ip.h>
#include <net/inet/util.h>
#include <net/inet/params.h>
#include <net/socket/socket.h>
#include <net/util.h>
#include <kern/kernlib.h>
#include <kern/lock.h>
#include <kern/thread.h>


#define NEED_PORT_ALLOC 0

struct udpcb {
  struct list_head link;
  struct queue_head recv_queue;
  struct sockaddr_in local_addr;
  struct sockaddr_in foreign_addr;
  mutex mtx;
};


static void *udp_sock_init();
static int udp_sock_bind(void *pcb, const struct sockaddr *addr);
static int udp_sock_close(void *pcb);
static int udp_sock_connect(void *pcb, const struct sockaddr *addr);
static int udp_sock_sendto(void *pcb, const u8 *msg, size_t len, int flags, struct sockaddr *dest_addr);
static size_t udp_analyze(struct pktbuf *pkt, struct sockaddr_in *addr);
static int udp_sock_recvfrom(void *pcb, u8 *buf, size_t len, int flags, struct sockaddr *from_addr);
static int udp_sock_send(void *pcb, const u8 *msg, size_t len, int flags);
static int udp_sock_recv(void *pcb, u8 *buf, size_t len, int flags);
static int udp_sock_listen(void *pcb, int backlog);
static void *udp_sock_accept(void *pcb, struct sockaddr *client_addr);

static const struct socket_ops udp_sock_ops = {
  .init = udp_sock_init,
  .bind = udp_sock_bind,
  .close = udp_sock_close,
  .connect = udp_sock_connect,
  .listen = udp_sock_listen,
  .accept = udp_sock_accept,
  .sendto = udp_sock_sendto,
  .recvfrom = udp_sock_recvfrom,
  .send = udp_sock_send,
  .recv = udp_sock_recv,
};

static struct list_head udpcb_list;

static mutex cblist_mtx;

#define UDPCB(s) (((struct udpcb *)(s))->pcb)

NET_INIT void udp_init() {
  list_init(&udpcb_list);

  mutex_init(&cblist_mtx);

  socket_register_ops(PF_INET, SOCK_DGRAM, &udp_sock_ops);
}

static int is_used_port(in_port_t port) {
  struct list_head *p;
  list_foreach(p, &udpcb_list) {
    struct udpcb *cb = list_entry(p, struct udpcb, link);
    if(cb->local_addr.port == port)
      return 1;
  }

  return 0;
}

static in_port_t get_unused_port() {
	for(in_port_t p=49152; p<65535; p++)
		if(!is_used_port(p))
			return p;

	return 0;
}

static u16 udp_checksum(struct ip_hdr *iphdr, struct udp_hdr *uhdr) {
  struct udp_pseudo_hdr pseudo;
  pseudo.up_src = iphdr->ip_src;
  pseudo.up_dst = iphdr->ip_dst;
  pseudo.up_type = 17;
  pseudo.up_void = 0;
  pseudo.up_len = uhdr->uh_ulen; //UDPヘッダ+UDPペイロードの長さ

  u16 sum = checksum2((u16*)(&pseudo), (u16*)uhdr, sizeof(struct udp_pseudo_hdr), ntoh16(uhdr->uh_ulen));
  return sum;
}

static void set_udpheader(struct udp_hdr *uhdr, u16 datalen, in_addr_t saddr, in_port_t sport, in_addr_t daddr, in_port_t dport){
  uhdr->uh_sport = sport;
  uhdr->uh_dport = dport;
  uhdr->uh_ulen = hton16(sizeof(struct udp_hdr) + datalen);
  uhdr->sum = 0;

  struct ip_hdr iphdr_tmp;
  iphdr_tmp.ip_src = saddr;
  iphdr_tmp.ip_dst = daddr;

  uhdr->sum = udp_checksum(&iphdr_tmp, uhdr);
  if(uhdr->sum == 0)
    uhdr->sum = 0xffff;

  return;
}

void udp_rx(struct pktbuf *pkt, struct ip_hdr *iphdr){
  struct udp_hdr *uhdr = (struct udp_hdr *)pkt->head;
  if(pktbuf_get_size(pkt) < sizeof(struct udp_hdr) ||
    pktbuf_get_size(pkt) < ntoh16(uhdr->uh_ulen)) {
    goto exit;
  }

  if(uhdr->sum != 0 && udp_checksum(iphdr, uhdr) != 0)
    goto exit;

  if(uhdr->uh_dport == 0)
    goto exit;

  struct udpcb *cb = NULL;
  struct list_head *p;
  mutex_lock(&cblist_mtx);
  list_foreach(p, &udpcb_list) {
    struct udpcb *b = list_entry(p, struct udpcb, link);
    if(b->local_addr.port != uhdr->uh_dport)
      continue;
    if(b->local_addr.addr != INADDR_ANY
        && b->local_addr.addr != iphdr->ip_dst)
      continue;

    cb = b;
  }

  if(cb == NULL)
    goto exit;
  else
    mutex_lock(&cb->mtx);

  mutex_unlock(&cblist_mtx);

  if(queue_is_full(&cb->recv_queue)) {
    pktbuf_free(list_entry(queue_dequeue(&cb->recv_queue), struct pktbuf, link));
  }
  pktbuf_add_header(pkt, ip_header_len(iphdr)); //IPヘッダも含める
  queue_enqueue(&pkt->link, &cb->recv_queue);
  mutex_unlock(&cb->mtx);
  thread_wakeup(cb);
  return;

exit:
  mutex_unlock(&cblist_mtx);
  pktbuf_free(pkt);
  return;
}


static void *udp_sock_init() {
  struct udpcb *cb = malloc(sizeof(struct udpcb));
  bzero(cb, sizeof(struct udpcb));
  mutex_init(&cb->mtx);
  queue_init(&cb->recv_queue, UDP_RECVQUEUE_LEN);

  mutex_lock(&cblist_mtx);
  list_pushback(&cb->link, &udpcb_list);
  mutex_unlock(&cblist_mtx);

  return cb;
}

static int udp_sock_bind(void *pcb, const struct sockaddr *addr) {
  struct udpcb *cb = (struct udpcb *)pcb;
  if(addr->family != PF_INET)
    return -1;

  struct sockaddr_in *inaddr = (struct sockaddr_in *)addr;
  mutex_lock(&cblist_mtx);
  if(inaddr->port == NEED_PORT_ALLOC)
    inaddr->port = get_unused_port();
  else if(is_used_port(inaddr->port)) {
    mutex_unlock(&cblist_mtx);
    return -1;
  }

  memcpy(&cb->local_addr, inaddr, sizeof(struct sockaddr_in));
  mutex_unlock(&cblist_mtx);
  return 0;
}

static int udp_sock_close(void *pcb) {
  struct udpcb *cb = (struct udpcb *)pcb;

  mutex_lock(&cblist_mtx);
  list_remove(&cb->link);
  mutex_unlock(&cblist_mtx);

  while(!queue_is_empty(&cb->recv_queue))
    pktbuf_free(list_entry(queue_dequeue(&cb->recv_queue), struct pktbuf, link));

  free(cb);
  return 0;
}

static int udp_sock_connect(void *pcb, const struct sockaddr *addr) {
  struct udpcb *cb = (struct udpcb *)pcb;
  if(addr->family != PF_INET)
    return -1;
  if(addr->addr == INADDR_ANY)
    return -1;

  struct sockaddr_in *inaddr = (struct sockaddr_in *)addr;
  if(inaddr->port == NEED_PORT_ALLOC)
    return -1;

  memcpy(&cb->foreign_addr, inaddr, sizeof(struct sockaddr_in));
  return 0;
}

static int udp_sock_sendto(void *pcb, const u8 *msg, size_t len, int flags UNUSED, struct sockaddr *dest_addr){
  struct udpcb *cb = (struct udpcb *)pcb;

  if(dest_addr->family != PF_INET)
    return -1;
  if(dest_addr->addr == INADDR_ANY)
    return -1;
  if(0xffff-sizeof(struct udp_hdr) < len)
    return -1;

  mutex_lock(&cblist_mtx);
  if(cb->local_addr.port == NEED_PORT_ALLOC)
    cb->local_addr.port = get_unused_port();
  struct sockaddr_in local_addr;
  memcpy(&local_addr, &cb->local_addr, sizeof(struct sockaddr_in));
  mutex_unlock(&cblist_mtx);

  in_addr_t r_src, r_dst;
  struct netdev *dev = ip_routing(local_addr.addr, ((struct sockaddr_in *)dest_addr)->addr, &r_src, &r_dst);
  if(dev == NULL)
    return -1; //no interface to send

  struct pktbuf *udpseg = pktbuf_alloc(MAX_HDRLEN_UDP + len);
  pktbuf_reserve_headroom(udpseg, MAX_HDRLEN_UDP);

  pktbuf_copyin(udpseg, msg, len, 0);
  set_udpheader((struct udp_hdr *)pktbuf_add_header(udpseg, sizeof(struct udp_hdr)), len,
    r_src, local_addr.port, r_dst, ((struct sockaddr_in *)dest_addr)->port);
  ip_tx(udpseg, r_src, r_dst, IPTYPE_UDP);

  return len;
}

static size_t udp_analyze(struct pktbuf *pkt, struct sockaddr_in *addr) {
  struct ip_hdr *iphdr = (struct ip_hdr *)pkt->head;
  struct udp_hdr *udphdr = (struct udp_hdr *)(((u8 *)iphdr)+ip_header_len(iphdr));
  if(addr != NULL) {
    addr->addr = iphdr->ip_src;
    addr->port = ntoh16(udphdr->uh_sport);
  }
  return ntoh16(udphdr->uh_ulen) - sizeof(struct udp_hdr);
}

static int udp_sock_recvfrom(void *pcb, u8 *buf, size_t len, int flags UNUSED, struct sockaddr *from_addr) {
  struct udpcb *cb = (struct udpcb *)pcb;
  mutex_lock(&cb->mtx);
  while(1) {
    if(queue_is_empty(&cb->recv_queue)) {
      mutex_unlock(&cb->mtx);
      thread_sleep(pcb);
    } else {
      struct pktbuf *pkt = list_entry(queue_dequeue(&cb->recv_queue), struct pktbuf, link);
      size_t datalen = udp_analyze(pkt, (struct sockaddr_in *)from_addr); //FIXME: check size of from_addr.
      pktbuf_remove_header(pkt, ip_header_len((struct ip_hdr *)pkt->head));
      pktbuf_remove_header(pkt, sizeof(struct udp_hdr));
      size_t copied = MIN(len, datalen);
      memcpy(buf, pkt->head, copied);
      pktbuf_free(pkt);
      mutex_unlock(&cb->mtx);
      return copied;
    }

    mutex_lock(&cb->mtx);
  }
}

static int udp_sock_send(void *pcb, const u8 *msg, size_t len, int flags) {
  struct udpcb *cb = (struct udpcb *)pcb;
  return udp_sock_sendto(pcb, msg, len, flags, (struct sockaddr *)(&cb->foreign_addr));
}

static int udp_sock_recv(void *pcb, u8 *buf, size_t len, int flags) {
  return udp_sock_recvfrom(pcb, buf, len, flags, NULL);
}

static int udp_sock_listen(void *pcb UNUSED, int backlog UNUSED) {
  return -1;
}

static void *udp_sock_accept(void *pcb UNUSED, struct sockaddr *client_addr UNUSED) {
  return NULL;
}
