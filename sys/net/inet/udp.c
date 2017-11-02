#include <net/inet/udp.h>
#include <net/inet/protohdr.h>
#include <net/inet/ip.h>
#include <net/inet/util.h>
#include <net/inet/params.h>
#include "errnolist.h"
#include <net/sockif.h>
#include <net/netlib.h>
#include <kern/kernlib.h>

static u16 udp_checksum(struct ip_hdr *iphdr, struct udp_hdr *uhdr){
  struct udp_pseudo_hdr pseudo;
  pseudo.up_src = iphdr->ip_src;
  pseudo.up_dst = iphdr->ip_dst;
  pseudo.up_type = 17;
  pseudo.up_void = 0;
  pseudo.up_len = uhdr->uh_ulen; //UDPヘッダ+UDPペイロードの長さ

  return checksum2((u16*)(&pseudo), (u16*)uhdr, sizeof(udp_pseudo_hdr), ntoh16(uhdr->uh_ulen));
}

void udp_rx(struct pktbuf_head *pkt, struct ip_hdr *iphdr){
  struct udp_hdr *uhdr = pkt->data;

  if(pkt->total < sizeof(struct udp_hdr) ||
    pkt->totale != ntoh16(uhdr->uh_ulen)){
    goto exit;
  }
  if(uhdr->sum != 0 && udp_checksum(iphdr, uhdr) != 0){
    goto exit;
  }

  wai_sem(UDP_SEM);

  int s;
  struct socket_t *sock;
  for(s=0; s<MAX_SOCKET; s++){
    sock = &sockets[s];
    if(sock->type==SOCK_DGRAM && sock->addr.my_port==ntoh16(uhdr->uh_dport))
      break;
  }

  if(s == MAX_SOCKET)
    goto exit;

  //キューに入れる
  udp_ctrlblock *ucb;
  ucb = sock->ctrlblock.ucb;
  wai_sem(UDP_RECV_SEM);
  ucb->recv_queue[ucb->recv_front] = flm;
  ucb->recv_front++;
  if(ucb->recv_front == DGRAM_RECV_QUEUE) ucb->recv_front=0;
  if(ucb->recv_front == ucb->recv_back){
    //キューがいっぱいなので、古いものから消す
    delete ucb->recv_queue[ucb->recv_front];
    ucb->recv_back++;
    if(ucb->recv_back == DGRAM_RECV_QUEUE) ucb->recv_back=0;
  }

  if(ucb->recv_waiting) wup_tsk(sock->ownertsk);
  sig_sem(UDP_RECV_SEM);

  sig_sem(UDP_SEM);
  return;
exit:
  sig_sem(UDP_SEM);
  return;
}

//UDPヘッダのチェックサム計算にはIPアドレスが必要
static void set_udpheader(struct udp_hdr *uhdr, u16 seglen, in_port_t sport, in_port_t dport, in_addr_t daddr){
  uhdr->uh_sport = hton16(sport);
  uhdr->uh_dport = hton16(dport);
  uhdr->uh_ulen = hton16(seglen);
  uhdr->sum = 0;

  //チェックサム計算用(送信元・宛先アドレスだけ埋めればいい)
  struct ip_hdr iphdr_tmp;
  iphdr_tmp.ip_src = IPADDR;
  iphdr_tmp.ip_dst = daddr;

  uhdr->sum = udp_checksum(&iphdr_tmp, uhdr);

  return;
}

int udp_sendto(const char *msg, size_t len, int flags, in_addr to_addr, in_port_t to_port, in_port_t my_port){
  //UDPで送れる最大サイズを超えている
  if(0xffff-sizeof(udp_hdr) < len) return EMSGSIZE;

  struct pktbuf_head *udpseg = new hdrstack(true);
  udpseg->next = NULL;
  udpseg->size=sizeof(udp_hdr)+len;
  udpseg->buf=new char[udpseg->size];
  memcpy(udpseg->buf+sizeof(udp_hdr), msg, len);

  set_udpheader((udp_hdr*)udpseg->buf,udpseg->size, my_port, to_port, to_addr);

  ip_send(udpseg, to_addr, IPTYPE_UDP);

  return len;
}

static char *udp_analyze(ether_flame *flm, u16 *datalen, u8 srcaddr[], u16 *srcport){
  ip_hdr *iphdr=(ip_hdr*)(ehdr+1);
  udp_hdr *udphdr=(udp_hdr*)(((u8*)iphdr)+(iphdr->ip_hl*4));
  *datalen = ntoh16(udphdr->uh_ulen)-sizeof(udp_hdr);
  if(srcaddr!=NULL) memcpy(srcaddr, iphdr->ip_src, IP_ADDR_LEN);
  if(srcport!=NULL) *srcport = ntoh16(udphdr->uh_sport);
  return (char*)(udphdr+1);
}

ssize_t udp_recvfrom(udp_ctrlblock *ucb, char *buf, size_t len, int flags, in_addr_t *from_addr, in_port_t *from_port, TMO timeout){
  wai_sem(UDP_RECV_SEM);
  //recv_waitingがtrueの時にデータグラムがやってきたら起こしてもらえる
  ucb->recv_waiting = true;
  while(true){
    if(ucb->recv_front == ucb->recv_back){
      sig_sem(UDP_RECV_SEM);
      if(tslp_tsk(timeout) == E_TMOUT){
        wai_sem(UDP_RECV_SEM);
        ucb->recv_waiting = false;
        sig_sem(UDP_RECV_SEM);
        return ETIMEOUT;
      }
    }else{
      u16 datalen;
      //char *data = udp_analyze(ucb->recv_queue[ucb->recv_back], &datalen, from_addr, from_port);
      struct pktbuf *pkt = recv_queue[ucb_recv_back];
      struct 
      memcpy(buf, data, MIN(len,datalen));
      delete ucb->recv_queue[ucb->recv_back];
      ucb->recv_back++;
      if(ucb->recv_back==DGRAM_RECV_QUEUE) ucb->recv_back=0;
      ucb->recv_waiting = false;
      sig_sem(UDP_RECV_SEM);
      return MIN(len,datalen);
    }

    wai_sem(UDP_RECV_SEM);
  }
}


