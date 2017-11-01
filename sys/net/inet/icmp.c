#include <net/inet/icmp.h>
#include <net/inet/protohdr.h>
#include <net/inet/ip.h>
#include <kern/types.h>

void icmp_rx(struct pktbuf_head *pkt, struct ip_hdr *iphdr){
  struct icmp *icmpdata = pkt->data;
  if( pkt->total < 4 /*ICMPヘッダのサイズ*/ ||
    checksum((u16*)icmpdata, pkt->total) != 0){
    goto exit;
  }

  switch(icmpdata->icmp_type){
  case ICMP_ECHO:
    icmpdata->icmp_type = ICMP_ECHOREPLY;
    icmpdata->icmp_cksum = 0;
    icmpdata->icmp_cksum = checksum((u16*)icmpdata, ip_datalen);
    ip_tx(pkt, iphdr->ip_src);
    break;
  }
  return;
}

