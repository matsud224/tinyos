#pragma once

#include <stdint.h>
#include <stddef.h>
#include "envdep.h"

//Ethernet
#define ETHER_ADDR_LEN 6

struct ether_hdr{
	uint8_t ether_dhost[ETHER_ADDR_LEN];
	uint8_t ether_shost[ETHER_ADDR_LEN];
	uint16_t ether_type;
};

//受信したものはEthernetフレームをそのままたらい回しにする
struct ether_flame{
	uint32_t size;
	char *buf;

	~ether_flame(){
		delete [] buf;
	}
};


//送信時に上位層からだんだんヘッダを重ねていくために使う
//   tcp/udpセグメント<-ipヘッダ<-ethernetヘッダ
//のように連結リストで管理して、送信時には順番に書き出す
//hdrstackという名前だが、ペイロードも統一して扱う
struct hdrstack{
	hdrstack *next;
	uint32_t size;
	char *buf;
	bool delete_needed; //デストラクタでbufをdeleteする責任を負うか

	hdrstack(bool delete_needed){
		this->delete_needed = delete_needed;
	}

	~hdrstack(){
		if(next!=NULL) delete next;
		if(delete_needed && buf!=NULL) delete [] buf;
	}
};

#define ETHERTYPE_IP	0x0800
#define ETHERTYPE_ARP	0x0806

//IP
#define IP_ADDR_LEN 4

struct ip_hdr{
#ifdef BIG_ENDIAN
	unsigned ip_v:4, ip_hl:4;
#endif //BIG_ENDIAN
#ifdef LITTLE_ENDIAN
	unsigned ip_hl:4, ip_v:4;
#endif // LITTLE_ENDIAN
	uint8_t ip_tos;
	uint16_t ip_len;
	uint16_t ip_id;
	uint16_t ip_off;
	uint8_t ip_ttl;
	uint8_t ip_p;
	uint16_t ip_sum;
	uint8_t ip_src[IP_ADDR_LEN];
	uint8_t ip_dst[IP_ADDR_LEN];
};

#define IP_RF 0x8000
#define IP_DE 0x4000
#define IP_MF 0x2000
#define IP_OFFMASK 0x1fff

#define IPTYPE_ICMP 1
#define IPTYPE_TCP 6
#define IPTYPE_UDP 17

//ARP
struct arp_hdr{
	uint16_t ar_hrd;
	uint16_t ar_pro;
	uint8_t	ar_hln;
	uint8_t ar_pln;
	uint16_t ar_op;
};

#define arp_hrd ea_hdr.ar_hrd
#define arp_pro ea_hdr.ar_pro
#define arp_hln ea_hdr.ar_hln
#define arp_pln ea_hdr.ar_pln
#define arp_op ea_hdr.ar_op

struct ether_arp{
	arp_hdr ea_hdr;
	uint8_t arp_sha[ETHER_ADDR_LEN];
	uint8_t arp_spa[IP_ADDR_LEN];
	uint8_t arp_tha[ETHER_ADDR_LEN];
	uint8_t arp_tpa[IP_ADDR_LEN];
};

#define ARPHRD_ETHER 1
#define ETHERTYPE_IP 0x0800

#define ARPOP_REQUEST 1
#define ARPOP_REPLY 2


//ICMP
struct icmp{
	uint8_t icmp_type;
	uint8_t icmp_code;
	uint16_t icmp_cksum;
	union{
		uint8_t ih_pptr;
		uint8_t ih_gwaddr[IP_ADDR_LEN];
		struct ih_idseq{
			int16_t icd_id;
			int16_t icd_seq;
		} ih_idseq;
		uint32_t ih_void;
		struct ih_pmtu{
			uint16_t ipm_void;
			uint16_t ipm_nextmtu;
		} ih_pmtu;

		struct ih_rtradv{
			uint8_t irt_num_addrs;
			uint8_t irt_wpa;
			uint16_t irt_lifetime;
		} ih_rtradv;
	} icmp_hun;

#define icmp_pptr imcp_hun.ih_pptr
#define icmp_gwaddr icmp_hun.ih_gwaddr
#define icmp_id icmp_hun.ih_idseq.icd_id
#define icmp_seq icmp_hun.ih_idseq.icd_seq
#define icmp_void icmp_hun.ih_void
#define icmp_pmvoid icmp_hun.ih_pmtu.ipm_void
#define icmp_nextmtu icmp_hun.ih_pmtu.ipm_nextmtu
#define icmp_num_addrs icmp_hun.ih_rtradv.irt_num_addrs
#define icmp_wpa icmp_hun.ih_rtradv.irt_wpa
#define icmp_lifetime icmp_hun.ih_rtradv.irt_lifetime

	union{
		struct id_ts{
			uint32_t its_otime;
			uint32_t its_rtime;
			uint32_t its_ttime;
		} id_ts;
		struct id_ip{
			struct ip_hdr idi_ip;
		} id_ip;
		struct icmp_ra_addr{
			uint32_t ira_addr;
			uint32_t ira_preference;
		} ip_radv;;
		uint32_t id_mask;
		uint8_t id_data[1];
	} icmp_dun;

#define icmp_otime icmp_dun.id_ts.its_otime
#define icmp_rtime icmp_dun.id_ts.its_rtime
#define icmp_ttime icmp_dun.id_ts.its_ttime
#define icmp_ip icmp_dun.id_ip.idi_ip
#define icmp_radv icmp_dun.id_radv
#define icmp_mask icmp_dun.id_mask
#define icmp_data icmp_dun.id_data

};

#define ICMP_ECHOREPLY 0
#define ICMP_UNREACH 3
#define ICMP_SOURCEQUENCH 4
#define ICMP_REDIRECT 5
#define ICMP_ECHO 8
#define ICMP_TIMXCEED 11
#define ICMP_PARAMPROB 12

#define ICMP_UNREACH_NET 0
#define ICMP_UNREACH_HOST 1
#define ICMP_UNREACH_PROTOCOL 2
#define ICMP_UNREACH_PORT 3
#define ICMP_UNREACH_NEEDFRAG 4
#define ICMP_UNREACH_SRCFAIL 5

#define ICMP_REDIRECT_NET 0
#define ICMP_REDIRECT_HOST 1
#define ICMP_REDIRECT_TOSNET 2
#define ICMP_REDIRECT_TOSHOST 3



//UDP
struct udp_hdr{
	uint16_t uh_sport;
	uint16_t uh_dport;
	uint16_t uh_ulen;
	uint16_t sum;
};

struct udp_pseudo_hdr{
	uint8_t up_src[IP_ADDR_LEN];
	uint8_t up_dst[IP_ADDR_LEN];
	uint8_t up_void;
	uint8_t up_type;
	uint16_t up_len;
};

struct tcp_pseudo_hdr{
	uint8_t tp_src[IP_ADDR_LEN];
	uint8_t tp_dst[IP_ADDR_LEN];
	uint8_t tp_void;
	uint8_t tp_type;
	uint16_t tp_len;
};


//TCP
struct tcp_hdr{
	uint16_t th_sport;
	uint16_t th_dport;
	uint32_t th_seq;
	uint32_t th_ack;
#ifdef BIG_ENDIAN
	unsigned th_off:4,th_x2:4;
#endif
#ifdef LITTLE_ENDIAN
	unsigned th_x2:4,th_off:4;
#endif // BYTE_ORDER
	uint8_t th_flags;
	uint16_t th_win;
	uint16_t th_sum;
	uint16_t th_urp;
};

#define TH_FIN 0x01
#define TH_SYN 0x02
#define TH_RST 0x04
#define TH_PUSH 0x08
#define TH_ACK 0x10
#define TH_URG 0x20
