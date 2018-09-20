#include <net/inet/ip.h>
#include <net/inet/arp.h>
#include <net/inet/udp.h>
#include <net/inet/tcp.h>
#include <net/inet/icmp.h>
#include <net/inet/util.h>
#include <net/util.h>
#include <net/inet/params.h>
#include <net/inet/protohdr.h>
#include <kern/lock.h>
#include <kern/kernlib.h>
#include <kern/pktbuf.h>
#include <kern/thread.h>
#include <kern/timer.h>

#define INF 0xffff


struct hole{
  struct list_head link;
  u16 first;
  u16 last;
};

struct fragment{
  struct list_head link;
  u16 first;
  u16 last;
  struct pktbuf *pkt;
};

struct hole *hole_new(u16 first, u16 last) {
  struct hole *h = malloc(sizeof(struct hole));
  h->first = first;
  h->last = last;
  return h;
}

struct fragment *fragment_new(u16 first, u16 last, struct pktbuf *pkt) {
  struct fragment *frag = malloc(sizeof(struct fragment));
  frag->first = first;
  frag->last = last;
  frag->pkt = pkt;
  return frag;
}

void fragment_free(struct fragment *f) {
  pktbuf_free(f->pkt);
  free(f);
}

struct reasminfo{
  struct list_head link;
  struct{
    in_addr_t ip_src;
    in_addr_t ip_dst;
    u8 ip_pro;
    u16 ip_id;
  } id;
  struct list_head holelist;
  struct list_head fraglist;
  struct pktbuf *head_frame;
  u8 headerlen; //etherヘッダ込
  u16 datalen;
  int16_t timeout; //タイムアウトまでのカウント
};

void reasminfo_free(struct reasminfo *ri) {
  list_free_all(&ri->holelist, struct hole, link, free);
  list_free_all(&ri->fraglist, struct fragment, link, fragment_free);
  free(ri);
}

static struct list_head reasm_ongoing;
static mutex reasm_ongoing_mtx;

void ip_10sec_thread(void *);

NET_INIT void ip_init(){
  list_init(&reasm_ongoing);
  mutex_init(&reasm_ongoing_mtx);
  thread_run(kthread_new(ip_10sec_thread, NULL));
}

void ip_10sec_thread(void *arg UNUSED) {
  while(1) {
    thread_set_alarm(ip_10sec_thread, msecs_to_ticks(10000));
    thread_sleep(ip_10sec_thread);
    mutex_lock(&reasm_ongoing_mtx);
    struct list_head *p, *tmp;
    list_foreach_safe(p, tmp, &reasm_ongoing) {
      struct reasminfo *info = list_entry(p, struct reasminfo, link);
      if(--(info->timeout) <= 0 || list_is_empty(&info->holelist)){
        list_remove(p);
        reasminfo_free(info);
      }
    }
    mutex_unlock(&reasm_ongoing_mtx);
  }
}

struct reasminfo *get_reasminfo(in_addr_t ip_src, in_addr_t ip_dst, u8 ip_pro, u16 ip_id){
  // already locked.
  struct list_head *p;
  list_foreach(p, &reasm_ongoing){
    struct reasminfo *ri = list_entry(p, struct reasminfo, link);
    if(ri->id.ip_src == ip_src && ri->id.ip_dst == ip_dst &&
       ri->id.ip_pro == ip_pro && ri->id.ip_id == ip_id &&
       ri->timeout > 0) {
      return ri;
    }
  }

  struct reasminfo *info = malloc(sizeof(struct reasminfo));
  info->id.ip_src = ip_src;
  info->id.ip_dst = ip_dst;
  info->id.ip_pro=ip_pro; info->id.ip_id=ip_id;
  info->timeout = IPFRAG_TIMEOUT_CLC;
  list_pushfront(&info->link, &reasm_ongoing);

  list_init(&info->holelist);
  list_init(&info->fraglist);

  list_pushfront(&hole_new(0, INF)->link, &info->holelist);
  info->head_frame = NULL;

  return info;
}


//データ領域のサイズが分かったので、last=無限大(0xffff)のホールを修正
void modify_inf_holelist(struct list_head *head, u16 newsize){
  struct list_head *p, *tmp;
  list_foreach_safe(p, tmp, head){
    struct hole *hole = list_entry(p, struct hole, link);
    if(hole->last == INF) {
      hole->last = newsize;
      if(hole->first == hole->last) {
        list_remove(p);
        free(hole);
      }
      return;
    }
  }
}

void ip_rx(struct pktbuf *pkt) {
  struct ip_hdr *iphdr = (struct ip_hdr *)pkt->head;
  u32 pktsize = pktbuf_get_size(pkt);
  if(pktsize < sizeof(struct ip_hdr))
    goto exit;
  if(pktsize < ntoh16(iphdr->ip_len))
    goto exit;

  if(iphdr->ip_v != 4)
    goto exit;
  if(iphdr->ip_hl < 5)
    goto exit;
  if(checksum((u16*)iphdr, ip_header_len(iphdr)) != 0)
    goto exit;

  //adjust pktbuf(Ethernet padding)
  if(ntoh16(iphdr->ip_len) < pktsize)
    pkt->tail -= pktsize - ntoh16(iphdr->ip_len);

  if(!((ntoh16(iphdr->ip_off) & IP_OFFMASK) == 0 && (ntoh16(iphdr->ip_off) & IP_MF) == 0)){
    //フラグメント
    mutex_lock(&reasm_ongoing_mtx);
    u16 ffirst = (ntoh16(iphdr->ip_off) & IP_OFFMASK)*8;
    u16 flast = ffirst + (ntoh16(iphdr->ip_len) - ip_header_len(iphdr)) - 1;

    struct reasminfo *info = get_reasminfo(iphdr->ip_src, iphdr->ip_dst, iphdr->ip_p, ntoh16(iphdr->ip_id));
    struct list_head *p, *tmp;

    list_foreach_safe(p, tmp, &(info->holelist)){
      struct hole *hole = list_entry(p, struct hole, link);
      u16 hfirst = hole->first;
      u16 hlast = hole->last;

      if(ffirst > hlast || flast < hfirst)
        continue;

      //タイムアウトを延長
      info->timeout = IPFRAG_TIMEOUT_CLC;

      if(ffirst > hfirst)
        list_insert_front(&hole_new(hfirst, ffirst-1)->link, p);
      if(flast < hlast)
        list_insert_front(&hole_new(flast+1, hlast)->link, p);

      list_pushfront(&fragment_new(ffirst, flast, pkt)->link, &info->fraglist);

      list_remove(p);
      free(hole);

      //more fragment がOFF
      if((ntoh16(iphdr->ip_off) & IP_MF) == 0) {
        info->datalen = flast+1;
        modify_inf_holelist(&(info->holelist), info->datalen);
      }
      //はじまりのフラグメント
      if(ffirst == 0) {
        info->head_frame = pkt;
        info->headerlen = ip_header_len(iphdr);
      }

      pktbuf_remove_header(pkt, ip_header_len(iphdr));
    }

    if(list_is_empty(&info->holelist)) {
      //フラグメントが揃った
      //printf("fragmented packet (%dbytes)\n", info->headerlen + info->datalen);
      pkt = pktbuf_alloc(info->headerlen + info->datalen);
      pktbuf_copyin(pkt, info->head_frame->head - info->headerlen, info->headerlen, 0);
      iphdr = (struct ip_hdr *)pkt->head;
      pktbuf_remove_header(pkt, info->headerlen);

      struct list_head *p;
      list_foreach(p, &info->fraglist) {
        struct fragment *f = list_entry(p, struct fragment, link);
        pktbuf_copyin(pkt, f->pkt->head, f->last - f->first +1, f->first);
      }
      info->timeout = 0;
    } else {
      mutex_unlock(&reasm_ongoing_mtx);
      return;
    }
    mutex_unlock(&reasm_ongoing_mtx);
  } else {
    pktbuf_remove_header(pkt, ip_header_len(iphdr));
  }

  switch(iphdr->ip_p){
  case IPTYPE_ICMP:
    icmp_rx(pkt, iphdr);
    break;
  case IPTYPE_TCP:
    tcp_rx(pkt, iphdr);
    break;
  case IPTYPE_UDP:
    udp_rx(pkt, iphdr);
    break;
  default:
    goto exit;
  }

  return;

exit:
  pktbuf_free(pkt);
}

void fill_iphdr(struct ip_hdr *iphdr, u16 datalen, u16 id,
          int mf, u16 offset, u8 proto, in_addr_t dstaddr, devno_t devno) {
  iphdr->ip_v = 4;
  iphdr->ip_hl = sizeof(struct ip_hdr)/4;
  iphdr->ip_tos = 0x80;
  iphdr->ip_len = hton16(sizeof(struct ip_hdr)+datalen);
  iphdr->ip_id = hton16(id);
  iphdr->ip_off = hton16((offset/8) | (mf?IP_MF:0));
  iphdr->ip_ttl = IP_TTL;
  iphdr->ip_p = proto;
  iphdr->ip_sum = 0;
  iphdr->ip_src = ((struct ifaddr_in *)netdev_find_addr(devno, PF_INET))->addr;
  iphdr->ip_dst = dstaddr;

  iphdr->ip_sum = checksum((u16*)iphdr, sizeof(struct ip_hdr));
  return;
}

static in_addr_t defaultgw;
void ip_set_defaultgw(in_addr_t addr) {
  defaultgw = addr;
}

in_addr_t ip_get_defaultgw() {
  return defaultgw;
}

int ip_routing_src(in_addr_t orig_src, in_addr_t orig_dst, in_addr_t *src, in_addr_t *dst, devno_t *devno) {
  struct list_head *p;
  list_foreach(p, &ifaddr_tbl[PF_INET]) {
    struct ifaddr_in *inaddr = list_entry(p, struct ifaddr_in, family_link);
    if(inaddr->addr == orig_src) {
      *src = orig_src;
      *dst = orig_dst;
      *devno = inaddr->devno;
      return 0;
    }
  }

  return -1;
}


int ip_routing(in_addr_t orig_src, in_addr_t orig_dst, in_addr_t *src, in_addr_t *dst, devno_t *devno) {
  if(orig_src != INADDR_ANY)
    return ip_routing_src(orig_src, orig_dst, src, dst, devno);

  struct list_head *p;
  list_foreach(p, &ifaddr_tbl[PF_INET]) {
    struct ifaddr_in *inaddr = list_entry(p, struct ifaddr_in, family_link);
    if((inaddr->addr & inaddr->netmask) == (orig_dst & inaddr->netmask)) {
      *src = inaddr->addr;
      *dst = orig_dst;
      *devno = inaddr->devno;
      return 0;
    }
  }

  if(orig_dst == defaultgw)
    return -1;

  return ip_routing(orig_src, defaultgw, src, dst, devno);
}

u16 ip_getid(){
  static u16 ip_id = 0;
  u16 newid;
  //ip_idをインクリメント
  while(newid = ip_id, newid != xchg(ip_id+1, &ip_id));
  return newid;
}

void ip_tx(struct pktbuf *data, in_addr_t srcaddr, in_addr_t dstaddr, u8 proto) {
  size_t datalen = pktbuf_get_size(data);
  size_t remainlen = datalen;
  u16 currentid = ip_getid();
  devno_t devno;
  in_addr_t r_src, r_dst;
  if(ip_routing(srcaddr, dstaddr, &r_src, &r_dst, &devno)) {
    //no interface available
    pktbuf_free(data);
    return;
  }

  if(sizeof(struct ip_hdr) + remainlen <= MTU){
    fill_iphdr((struct ip_hdr *)pktbuf_add_header(data, sizeof(struct ip_hdr)),
                 remainlen, currentid, 0, 0, proto, r_dst, devno);
    arp_tx(data, r_dst, ETHERTYPE_IP, devno);
  }else{
    while(remainlen > 0) {
      size_t frag_totallen =
        MIN(remainlen + sizeof(struct ip_hdr), MTU);
      size_t frag_datalen = frag_totallen - sizeof(struct ip_hdr);
      u8 offset = datalen - remainlen;
      struct pktbuf *pkt = pktbuf_alloc(sizeof(struct ether_hdr) + frag_totallen);
      pktbuf_reserve_headroom(pkt, sizeof(struct ether_hdr) + sizeof(struct ip_hdr));
      pktbuf_copyin(pkt, data->head + offset, frag_datalen, 0);
      fill_iphdr((struct ip_hdr *)pktbuf_add_header(pkt, sizeof(struct ip_hdr)), frag_datalen,
                   currentid, remainlen>0, offset, proto, r_dst, devno);

      arp_tx(pkt, r_dst, ETHERTYPE_IP, devno);
      remainlen -= frag_datalen;
    }
  }
}
