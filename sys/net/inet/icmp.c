#include <net/inet/icmp.h>
#include <net/inet/protohdr.h>
#include <net/inet/ip.h>
#include <net/inet/util.h>
#include <kern/types.h>

void icmp_rx(struct pktbuf *pkt, struct ip_hdr *iphdr){
  struct icmp *icmpdata = (struct icmp *)pkt->head;
  size_t pktsize = pktbuf_get_size(pkt);
  if( pktsize < 4 /*ICMPヘッダのサイズ*/ ||
    checksum((u16*)icmpdata, pktsize) != 0){
    goto exit;
  }
puts("ICMP packet received.");
  switch(icmpdata->icmp_type){
  case ICMP_ECHO:
    icmpdata->icmp_type = ICMP_ECHOREPLY;
    icmpdata->icmp_cksum = 0;
    icmpdata->icmp_cksum = checksum((u16*)icmpdata, pktsize);
    ip_tx(pkt, iphdr->ip_src, IPTYPE_ICMP);
    break;
  }
  return;

exit:
  pktbuf_free(pkt);
}

