#include "ethernet.h"
#include "util.h"
#include "protohdr.h"

void ethernet_rx(struct pktbuf_head *frame) {
  if(frame->total < sizeof(ether_hdr))
    goto reject;

  struct ether_hdr *ehdr = (struct ether_hdr *)frame->data;
  if(memcmp(ehdr->ether_dhost, MACADDR, ETHER_ADDR_LEN)!=0 &&
    memcmp(ehdr->ether_dhost, ETHERBROADCAST, ETHER_ADDR_LEN)!=0){
    goto reject;
  }

  pktbuf_remove_header(frame, sizeof(struct ether_hdr));
  switch(ntoh16(ehdr->ether_type)){
  case ETHERTYPE_IP:
		ip_rx(frame);
		break;
  case ETHERTYPE_ARP:
		arp_rx(frame);
    break;
  default:
    goto reject;
    break;
  }

  return;

reject:
  pkt_free(frame);
  return;
}

void ethernet_tx(struct pktbuf_head *frame){
  netdev_tx(0, frame);
  return;
}

