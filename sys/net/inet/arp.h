#pragma once
#include <net/inet/protohdr.h>

struct arpentry{
	etheraddr macaddr;
	in_addr_t ipaddr;
	u16 timeout;
#define ARPTBL_PERMANENT 0xffff //timeoutをこの値にした時はタイムアウトしない
	struct list_head pending; //アドレス解決待ちのフレーム
};

extern struct arpentry arptable[MAX_ARPTABLE];

void arp_init(void);
void arp_rx(struct pktbuf_head *frm, struct ether_arp *earp);
void arp_tx(struct pktbuf_head *packet, u8 dstaddr[], u16 proto);
void register_arptable(in_addr_t ipaddr, struct etheraddr macaddr, bool is_permanent);
struct pktbuf_head *make_arprequest_frame(etheraddr dstaddr[]);

