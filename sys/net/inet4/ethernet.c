#include "ethernet.h"
#include "util.h"
#include "protohdr.h"
#include "pktbuf.h"

#define ETHER_RX_MAX 16 //一度に処理するフレーム数

void ethernet_rx(void *_devno) {
  int remain = ETHER_RX_MAX;
  devno_t devno = (devno_t)_devno;
  struct pktbuf_head *frame = NULL;
  while(--remain && (frame = netdev_rx_nowait(devno)) != NULL)
    ethernet_rx_one(frame);
  if(remain == 0)
    defer_exec(ethernet_rx, devno, 1);
}

void ethernet_rx_one(struct pktbuf_head *frame) {
  if(frame->total < sizeof(struct ether_hdr))
    goto reject;

  struct ether_hdr *ehdr = (struct ether_hdr *)frame->data;
/*  if(memcmp(ehdr->ether_dhost, MACADDR, ETHER_ADDR_LEN)!=0 &&
    memcmp(ehdr->ether_dhost, ETHERBROADCAST, ETHER_ADDR_LEN)!=0){
    goto reject;
  }
*/
  pktbuf_remove_header(frame, sizeof(struct ether_hdr));
  switch(ntoh16(ehdr->ether_type)){
  case ETHERTYPE_IP:
    puts("ip packet");
  pktbuf_free(frame);
		//ip_rx(frame);
		break;
  case ETHERTYPE_ARP:
    puts("arp packet");
  pktbuf_free(frame);
		//arp_rx(frame);
    break;
  default:
    goto reject;
    break;
  }

  return;

reject:
  pktbuf_free(frame);
  return;
}

void ethernet_tx(struct pktbuf_head *frame){
  netdev_tx(0, frame);
  return;
}

