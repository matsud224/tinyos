#include <net/ether/ether.h>
#include <net/ether/protohdr.h>
#include <net/inet/arp.h>
#include <net/inet/ip.h>
#include <net/util.h>
#include <kern/pktbuf.h>
#include <kern/task.h>

#define ETHER_RX_MAX 16 //一度に処理するフレーム数

const struct etheraddr ETHER_ADDR_BROADCAST = {
  .addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}
};

static void ether_rx_one(struct pktbuf *frame);

void ether_rx(void *_devno) {
  int remain = ETHER_RX_MAX;
  devno_t devno = (devno_t)_devno;
  struct pktbuf *frame = NULL;
  while(--remain && (frame = netdev_rx_nowait(devno)) != NULL)
    ether_rx_one(frame);
  if(remain == 0)
    defer_exec(ether_rx, (void *)devno, 0, 1);
}

static void ether_rx_one(struct pktbuf *frame) {
  if(pktbuf_get_size(frame) < sizeof(struct ether_hdr))
    goto reject;

  struct ether_hdr *ehdr = (struct ether_hdr *)frame->head;
  pktbuf_remove_header(frame, sizeof(struct ether_hdr));
  switch(ntoh16(ehdr->ether_type)){
  case ETHERTYPE_IP:
    puts("ip packet");
    ip_rx(frame);
    break;
  case ETHERTYPE_ARP:
    puts("arp packet");
    arp_rx(frame);
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

void ether_tx(struct pktbuf *frm, struct etheraddr dest, u16 proto, struct netdev *dev){
  struct ether_hdr *ehdr = (struct ether_hdr *)pktbuf_add_header(frm, sizeof(struct ether_hdr));
  ehdr->ether_type = hton16(proto);
  ehdr->ether_dhost = dest;
  ehdr->ether_shost = *(struct etheraddr *)netdev_find_addr(dev, PF_LINK)->addr;
  netdev_tx(dev->devno, frm);
  return;
}

