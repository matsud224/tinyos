#pragma once
#include "protohdr.h"
#include "netconf.h"

struct arpentry{
	u8 macaddr[ETHER_ADDR_LEN];
	u32 ipaddr; //ネットワークバイトオーダ
	u16 timeout;
#define ARPTBL_PERMANENT 0xffff //timeoutをこの値にした時はタイムアウトしない
	struct list_head pending; //アドレス解決待ちのフレーム
};

extern struct arpentry arptable[MAX_ARPTABLE];

void arp_init(void);
void arp_rx(struct pktbuf_head *frm, ether_arp *earp);
void arp_tx(struct pktbuf_head *packet, u8 dstaddr[], u16 proto);
void register_arptable(u32 ipaddr, u8 macaddr[], bool is_permanent);
struct pktbuf_head *make_arprequest_frame(u8 dstaddr[]);

