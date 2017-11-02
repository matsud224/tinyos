#include <net/inet/ip.h>
#include <net/inet/arp.h>
#include <net/inet/icmp.h>
#include <net/inet/util.h>
#include <net/inet/params.h>
#include <net/inet/protohdr.h>

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
  struct pktbuf_head *frm;
};

static struct hole *hole_new(u16 first, u16 last) {
  struct hole *h = malloc(sizeof(struct hole));
  h->first = first;
  h->last = last;
  return h;
}

static struct fragment *fragment_new(u16 first, u16 last, struct pktbuf_head *frm) {
  struct fragment *frag = malloc(sizeof(struct fragment));
  frag->first = first;
  frag->last = last;
  frag->frm = frm;
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
  struct pktbuf_head *head_frame;
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
  pktbuf_free(head_frame);
  free(ri);
}

static struct list_head reasm_ongoing;

void start_ip(){
  list_init(&reasm_ongoing);
  act_tsk(ETHERRECV_TASK);
  act_tsk(TIMEOUT_10SEC_TASK);
  sta_cyc(TIMEOUT_10SEC_CYC);
}

void ip_10sec() {
  //10sec周期で動き、タイムアウトを管理するタスク
  while(true){
    //IPフラグメント組み立てタイムアウト
    wai_sem(TIMEOUT_10SEC_SEM);
    struct reasminfo *p, *tmp;
    list_foreach_safe(p, tmp, &reasm_ongoing) {
      struct reasminfo *info = list_entry(p, struct reasminfo, link);
      if(--(info->timeout) <= 0 || list_is_empty(info->holelist)){
        list_remove(p);
        reasminfo_free(info);
      }
    }
    sig_sem(TIMEOUT_10SEC_SEM);

    //ARPテーブル
    wai_sem(ARPTBL_SEM);
    for(int i=0; i<MAX_ARPTABLE; i++){
      if(arptable[i].timeout>0 && arptable[i].timeout!=ARPTBL_PERMANENT){
        arptable[i].timeout--;
      }
      if(arptable[i].timeout == 0){
        if(arptable[i].pending!=NULL){
          delete arptable[i].pending;
          arptable[i].pending = NULL;
        }
      }else{
        if(arptable[i].pending!=NULL){
          ether_flame *request = make_arprequest_frame((u8*)(&arptable[i].ipaddr));
          ethernet_send(request);
          delete request;
        }
      }
    }
    sig_sem(ARPTBL_SEM);
  }
  
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

void ip_rx(struct pktbuf_head *pkt){
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
  if(checksum((u16*)iphdr, iphdr->ip_hl*4) != 0)
    goto exit;

  if(!((ntoh16(iphdr->ip_off) & IP_OFFMASK) == 0 && (ntoh16(iphdr->ip_off) & IP_MF) == 0)){
    //フラグメント
    wai_sem(TIMEOUT_10SEC_SEM);
    u16 ffirst = (ntoh16(iphdr->ip_off) & IP_OFFMASK)*8;
    u16 flast = ffirst + (ntoh16(iphdr->ip_len) - iphdr->ip_hl*4) - 1;

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

      list_pushfront(fragment_new(ffirst, flast, frm), info->fraglist);
      list_remove(p);
      free(hole);

      //more fragment がOFF
      if((ntoh16(iphdr->ip_off) & IP_MF) == 0) {
        info->datalen = flast+1;
        modify_inf_holelist(&(info->holelist), info->datalen);
      }
      //はじまりのフラグメント
      if(ffirst == 0) {
        info->head_frame = frm;
        info->headerlen = sizeof(ether_hdr) + (iphdr->ip_hl*4);
      }
    }

    if(list_is_empty(&info->holelist)) {
      //holeがなくなり、おしまい
      //パケットを構築
      frm = pktbuf_alloc(info->headerlen + info->datalen);
      memcpy(frm->data, info->head_frame->buf, info->headerlen);
      char *origin = frm->buf + info->headerlen;
      int total = 0;
      struct list_head *p;
      list_foreach(p, info->fraglist) {
        struct fragment *f = list_entry(p, struct fragment, link);
        memcpy(origin+f->first, ((u8*)(f->frm->buf)) + info->headerlen, f->last - fptr->first + 1);
        total += fptr->last - fptr->first + 1;
      }
      //frmを上書きしたので、iphdrも修正必要
      iphdr = (ip_hdr*)(frm->buf+sizeof(ether_hdr));
      info->timeout = 0;
    } else {
      sig_sem(TIMEOUT_10SEC_SEM);
      return;
    }
    sig_sem(TIMEOUT_10SEC_SEM);
  }

  switch(iphdr->ip_p){
  case IPTYPE_ICMP:
    icmp_rx(pkt, iphdr);
    break;
  case IPTYPE_TCP:
    //tcp_rx(frm, iphdr, (tcp_hdr*)(((u8*)iphdr)+(iphdr->ip_hl*4)));
    break;
  case IPTYPE_UDP:
    udp_rx(frm, iphdr);
    break;
  }

  return;
}

static u16 ip_id = 0;

static void prep_iphdr(ip_hdr *iphdr, u16 len, u16 id,
          bool mf, u16 offset, u8 proto, in_addr_t dstaddr){
  iphdr->ip_v = 4;
  iphdr->ip_hl = sizeof(ip_hdr)/4;
  iphdr->ip_tos = 0x80;
  iphdr->ip_len = hton16(len);
  iphdr->ip_id = hton16(id);
  iphdr->ip_off = hton16((offset/8) | (mf?IP_MF:0));
  iphdr->ip_ttl = IP_TTL;
  iphdr->ip_p = proto;
  iphdr->ip_sum = 0;
  iphdr->ip_src =  IPADDR;
  iphdr->ip_dst =  dstaddr;

  iphdr->ip_sum = checksum((u16*)iphdr, sizeof(ip_hdr));
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
  u16 id;
  wai_sem(IPID_SEM);
  id = ip_id; ip_id++;
  sig_sem(IPID_SEM);
  return id;
}

void ip_tx(struct pktbuf_head *data, in_addr_t dstaddr, u8 proto){
  u32 datalen = data->total;
  u32 remainlen = datalen;
  u16 currentid = ip_getid();

  //ルーティング
  in_addr_t dstaddr_r;
  dstaddr_r = ip_routing(dstaddr_r);

  if(sizeof(ip_hdr)+remainlen <= MTU){
    hdrstack *iphdr_item = new hdrstack(true);
    iphdr_item->size = sizeof(ip_hdr);
    iphdr_item->buf = new char[sizeof(ip_hdr)];
    prep_iphdr((ip_hdr*)iphdr_item->buf, sizeof(ip_hdr)+remainlen, currentid, false, 0, proto, dstaddr);
    iphdr_item->next = data;
    arp_send(iphdr_item, dstaddr_r, ETHERTYPE_IP);
  }else{
    while(remainlen > 0){
      u32 thispkt_totallen = 
        MIN(remainlen + sizeof(struct ip_hdr), MTU);
      u32 thispkt_datalen = thispkt_totallen - sizeof(struct ip_hdr);
      u16 offset = datalen - remainlen;
      struct pktbuf *pkt = pktbuf_alloc();
      pkt->next = NULL;
      remainlen -= thispkt_datalen;
      pkt->size = thispkt_totallen;
      pkt->buf = new char[ippkt->size];
      prep_iphdr((ip_hdr*)ippkt->buf, thispkt_totallen, currentid, (remainlen>0)?true:false
            , offset, proto, dstaddr);
      hdrstack_cpy((char*)(((ip_hdr*)ippkt->buf)+1), data, offset, thispkt_datalen);

      arp_send(pkt, dstaddr_r, ETHERTYPE_IP);
    }
  }
}
