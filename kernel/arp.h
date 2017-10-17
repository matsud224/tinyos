#pragma once
#include "protohdr.h"
#include "netconf.h"

struct flist{
	flist *next;
	hdrstack *flm;
	~flist(){
		if(next!=NULL) delete next;
		delete flm;
	}
};

struct arpentry{
	uint8_t macaddr[ETHER_ADDR_LEN];
	uint32_t ipaddr; //ネットワークバイトオーダ
	uint16_t timeout;
#define ARPTBL_PERMANENT 0xffff //timeoutをこの値にした時はタイムアウトしない
	flist *pending; //アドレス解決待ちのフレーム
};

extern arpentry arptable[MAX_ARPTABLE];

void arp_process(ether_flame *flm, ether_arp *earp);
void arp_send(hdrstack *packet, uint8_t dstaddr[], uint16_t proto);
void register_arptable(uint32_t ipaddr, uint8_t macaddr[], bool is_permanent);
ether_flame *make_arprequest_flame(uint8_t dstaddr[]);

