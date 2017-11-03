#include <net/inet/ip.h>
#include <net/inet/arp.h>
#include <net/inet/icmp.h>
#include <net/inet/util.h>
#include <net/inet/params.h>
#include <net/inet/protohdr.h>
#include <kern/lock.h>
#include <kern/kernlib.h>
#include <kern/pktbuf.h>

#define INF 0xffff

#define ip_header_len(iphdr) (iphdr->ip_hl*4)

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

static struct hole *hole_new(u16 first, u16 last) {
  struct hole *h = malloc(sizeof(struct hole));
  h->first = first;
  h->last = last;
  return h;
}

static struct fragment *fragment_new(u16 first, u16 last, struct pktbuf *pkt) {
  struct fragment *frag = malloc(sizeof(struct fragment));
  frag->first = first;
  frag->last = last;
  frag->pkt = pkt;
  return frag;
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

static void reasminfo_free(struct reasminfo *ri) {
  struct list_head *p, *tmp;
  list_foreach_safe(p, tmp, &ri->holelist) {
    struct hole *h = list_entry(p, struct hole, link);
    free(h);
  }
  list_foreach_safe(p, tmp, &ri->holelist) {
    struct fragment *f = list_entry(p, struct fragment, link);
    pktbuf_free(f->frm);
    free(f);
  }
  free(ri);
}

static struct list_head reasm_ongoing;
static mutex reasm_ongoing_mtx;

NETINIT void ip_init(){
  list_init(&reasm_ongoing);
  mutex_init(&reasm_ongoing_mtx);
}

void ip_10sec() {
  mutex_lock(&reasm_ongoing_lock);
  struct reasminfo *p, *tmp;
  list_foreach_safe(p, tmp, &reasm_ongoing) {
    struct reasminfo *info = list_entry(p, struct reasminfo, link);
    if(--(info->timeout) <= 0 || list_is_empty(info->holelist)){
      list_remove(p);
      reasminfo_free(info);
    }
  }
  mutex_unlock(&reasm_ongoing_lock);
  
  defer_exec(ip_10sec, 
}

static reasminfo *get_reasminfo(in_addr_t ip_src, in_addr_t ip_dst, u8 ip_pro, u16 ip_id){
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
  list_pushfront(info, &reasm_ongoing);

  list_init(&info->holelist);
  list_init(&info->fraglist);

  list_pushfront(hole_new(0, INF), &info->holelist);
  info->head_frame = NULL;

  return info;
}


//データ領域のサイズが分かったので、last=無限大(0xffff)のホールを修正
static void modify_inf_holelist(struct list_head *head, u16 newsize){
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

void ip_rx(struct pktbuf *pkt){
  struct ip_hdr *iphdr = pkt->data;
  //正しいヘッダかチェック
  if(pkt->total < sizeof(struct ip_hdr))
    goto exit;
  if(pkt->total < ntoh16(iphdr->ip_len))
    goto exit;

  if(iphdr->ip_v != 4)
    goto exit;
  if(iphdr->ip_hl < 5){
    goto exit;
  if(checksum((u16*)iphdr, ip_header_len(iphdr)) != 0)
    goto exit;

  if(!((ntoh16(iphdr->ip_off) & IP_OFFMASK) == 0 && (ntoh16(iphdr->ip_off) & IP_MF) == 0)){
    //フラグメント
    mutex_lock(&reasm_ongoing_lock);
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
        list_insert_front(hole_new(hfirst, ffirst-1), p);
      if(flast < hlast)
        list_insert_front(hole_new(flast+1, hlast), p);

      list_pushfront(fragment_new(ffirst, flast, pkt), info->fraglist);

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
        info->headerlen = sizeof(ether_hdr) + ip_header_len(iphdr);
      }

      pktbuf_remove_header(pkt, ip_header_len(iphdr));
    }

    if(list_is_empty(&info->holelist)) {
      //フラグメントが揃った
      pkt = pktbuf_alloc(info->headerlen + info->datalen);
      pktbuf_copyin(pkt, info->head_frame->head - info->headerlen, info->headerlen, 0);
      iphdr = (struct ip_hdr *)pktbuf->head;
      pktbuf_remove_header(pkt, info->headerlen);

      struct list_head *p;
      list_foreach(p, info->fraglist) {
        struct fragment *f = list_entry(p, struct fragment, link);
        pktbuf_copyin(pkt, f->pkt->data, f->last - f->first +1, f->first);
      }

      info->timeout = 0;
    } else {
      mutex_unlock(&reasm_ongoing_lock);
      return;
    }
    mutex_unlock(&reasm_ongoing_lock);
  } else {
    pktbuf_remove_header(pkt, info->headerlen);
  }

  switch(iphdr->ip_p){
  case IPTYPE_ICMP:
    icmp_rx(pkt, iphdr);
    break;
  case IPTYPE_TCP:
    //tcp_rx(pkt, iphdr);
    break;
  case IPTYPE_UDP:
    udp_rx(pkt, iphdr);
    break;
  }

  return;
}


static void fill_iphdr(struct ip_hdr *iphdr, u16 datalen, u16 id,
          int mf, u16 offset, u8 proto, in_addr_t dstaddr){
  iphdr->ip_v = 4;
  iphdr->ip_hl = sizeof(struct ip_hdr)/4;
  iphdr->ip_tos = 0x80;
  iphdr->ip_len = hton16(sizeof(struct ip_hdr)+datalen);
  iphdr->ip_id = hton16(id);
  iphdr->ip_off = hton16((offset/8) | (mf?IP_MF:0));
  iphdr->ip_ttl = IP_TTL;
  iphdr->ip_p = proto;
  iphdr->ip_sum = 0;
  iphdr->ip_src =  IPADDR;
  iphdr->ip_dst =  dstaddr;

  iphdr->ip_sum = checksum((u16*)iphdr, sizeof(struct ip_hdr));
  return;
}

in_addr_t ip_routing(in_addr_t dstaddr){
  in_addr_t myaddr=IPADDR_TO_UINT32(IPADDR);
  in_addr_t mymask=IPADDR_TO_UINT32(NETMASK);
  in_addr_t dst = IPADDR_TO_UINT32(dstaddr);
  if(dst!=0 && (myaddr&mymask)!=(dst&mymask) && dstaddr != IPBROADCAST){
    //同一のネットワークでない->デフォルトゲートウェイに流す
    dstaddr = DEFAULT_GATEWAY;
  }

  return dstaddr;
}

u16 ip_getid(){
  static u16 ip_id = 0;
  u16 newid;
  while(newid = ip_id, newid != xchg(ip_id+1, &ip_id));
  return newid;
}

void ip_tx(struct pktbuf *data, in_addr_t dstaddr, u8 proto){
  size_t datalen = pktbuf_get_size(data);
  size_t remainlen = datalen;
  u16 currentid = ip_getid();

  dstaddr = ip_routing(dstaddr);

  if(sizeof(struct ip_hdr) + remainlen <= MTU){
    fill_iphdr((struct ip_hdr *)pktbuf_add_header(data, sizeof(struct ip_hdr)), 
                 remainlen, currentid, 0, 0, proto, dstaddr);
    arp_send(data, dstaddr_r, ETHERTYPE_IP);
  }else{
    while(remainlen > 0) {
      size_t frag_totallen = 
        MIN(remainlen + sizeof(struct ip_hdr), MTU);
      size_t frag_datalen = frag_totallen - sizeof(struct ip_hdr);
      off_t offset = datalen - remainlen;
      struct pktbuf *pkt = pktbuf_alloc(sizeof(struct ether_hdr) + frag_totallen);
      pktbuf_reserve_headroom(sizeof(struct ether_hdr) + sizeof(struct ip_hdr));
      pktbuf_copyin(pkt, data->head + offset, frag_datalen, 0);
      fill_iphdr((struct ip_hdr *)pktbuf_add_header(pkt, sizeof(struct ip_hdr)), frag_datalen, 
                   currentid, remainlen>0, offset, proto, dstaddr);

      arp_send(pkt, dstaddr, ETHERTYPE_IP);
      remainlen -= frag_datalen;
    }
  }
}
