#include <cstring>

#include "netconf.h"
#include "icmp.h"
#include "protohdr.h"
#include "util.h"
#include "ip.h"
#include "ethernet.h"

#define MIN(x,y) ((x)<(y)?(x):(y))

//ICMPエコー要求メッセージを加工して応答にする
static void make_icmp_echoreply(ether_flame *flm, ip_hdr *iphdr, icmp *icmpdata){
	ether_hdr *ehdr = (ether_hdr*)flm->buf;
	uint8_t mac_temp[ETHER_ADDR_LEN];
	uint8_t ip_temp[IP_ADDR_LEN];
	//送信元と宛先を入れ替え
	memcpy(mac_temp, ehdr->ether_shost, ETHER_ADDR_LEN);
	memcpy(ehdr->ether_shost, ehdr->ether_dhost, ETHER_ADDR_LEN);
	memcpy(ehdr->ether_dhost, mac_temp, ETHER_ADDR_LEN);
	//IPヘッダも同様に修正
	uint16_t ip_datalen = MIN(MTU-iphdr->ip_hl*4, flm->size-sizeof(ether_hdr)-iphdr->ip_hl*4);
    iphdr->ip_len = hton16(ip_datalen + iphdr->ip_hl*4);
    iphdr->ip_id = hton16(ip_getid());
    iphdr->ip_ttl = IP_TTL;
    memcpy(ip_temp, iphdr->ip_src, IP_ADDR_LEN);
	memcpy(iphdr->ip_src, iphdr->ip_dst, IP_ADDR_LEN);
	memcpy(iphdr->ip_dst, ip_temp, IP_ADDR_LEN);
	iphdr->ip_sum = 0;
	iphdr->ip_sum = checksum((uint16_t*)iphdr, iphdr->ip_hl*4);
	//ICMPのデータ部が長い時は、MTUに収まるように切り詰める（手抜き）
	flm->size = ip_datalen + iphdr->ip_hl*4 + sizeof(ether_hdr);
	//ICMPヘッダの加工
	icmpdata->icmp_type = ICMP_ECHOREPLY;
	icmpdata->icmp_cksum = 0;
	icmpdata->icmp_cksum = checksum((uint16_t*)icmpdata, ip_datalen);
	return;
}

void icmp_process(ether_flame *flm, ip_hdr *iphdr, icmp *icmpdata){
	if( flm->size-sizeof(ether_hdr)-iphdr->ip_hl*4 < 4 /*共通部分のサイズ*/ ||
		checksum((uint16_t*)icmpdata, flm->size-sizeof(ether_hdr)-iphdr->ip_hl*4) != 0){
		goto exit;
	}

	switch(icmpdata->icmp_type){
	case ICMP_ECHO:
		make_icmp_echoreply(flm, iphdr, icmpdata);
		ethernet_send(flm);
		break;
	}

exit:
	delete flm;
	return;
}

