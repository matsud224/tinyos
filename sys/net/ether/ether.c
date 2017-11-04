#include <net/ether/ether.h>
#include <net/ether/protohdr.h>
#include <kern/pktbuf.h>

#define ETHER_RX_MAX 16 //一度に処理するフレーム数

void ethernet_rx(void *_devno) {
  int remain = ETHER_RX_MAX;
  devno_t devno = (devno_t)_devno;
  struct pktbuf *frame = NULL;
  while(--remain && (frame = netdev_rx_nowait(devno)) != NULL)
    ethernet_rx_one(frame);
  if(remain == 0)
    defer_exec(ethernet_rx, devno, 1);
}

static void ethernet_rx_one(struct pktbuf *frame) {
  if(frame->total < sizeof(struct ether_hdr))
    goto reject;

  struct ether_hdr *ehdr = (struct ether_hdr *)frame->data;
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

static etheraddr find_link_addr(struct netdev *dev) {
  struct list_head *p;
  list_foreach(p, &dev->addr_list) {
    struct inaddr *addr = list_entry(p, struct inaddr, link);
    if(addr->family == PF_LINK)
      return addr->addr;
  }
  return 0;
}

void ethernet_tx(struct pktbuf *frm, struct ether_addr dest, u16 proto, struct netdev *dev){
  struct ether_hdr *edr = pktbuf_add_header(frm, sizeof(struct ether_hdr);
  ehdr->ether_type = hton16(proto);
  ehdr->ether_dhost = dest;
  ehdr->ether_shost = find_link_addr(dev);
  netdev_tx(dev->devno, frm);
  return;
}

