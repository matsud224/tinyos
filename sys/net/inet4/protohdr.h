#pragma once
#include "types.h"
#include "envdep.h"

//Ethernet
#define ETHER_ADDR_LEN 6

struct ether_hdr{
	u8 ether_dhost[ETHER_ADDR_LEN];
	u8 ether_shost[ETHER_ADDR_LEN];
	u16 ether_type;
};
#define ETHERTYPE_IP	0x0800
#define ETHERTYPE_ARP	0x0806

//IPv4
#define IPV4_ADDR_LEN 4

struct ipv4_hdr{
#ifdef BIG_ENDIAN
	unsigned ip_v:4, ip_hl:4;
#endif //BIG_ENDIAN
#ifdef LITTLE_ENDIAN
	unsigned ip_hl:4, ip_v:4;
#endif // LITTLE_ENDIAN
	u8 ip_tos;
	u16 ip_len;
	u16 ip_id;
	u16 ip_off;
	u8 ip_ttl;
	u8 ip_p;
	u16 ip_sum;
	u8 ip_src[IP_ADDR_LEN];
	u8 ip_dst[IP_ADDR_LEN];
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
	u16 ar_hrd;
	u16 ar_pro;
	u8	ar_hln;
	u8 ar_pln;
	u16 ar_op;
};

#define arp_hrd ea_hdr.ar_hrd
#define arp_pro ea_hdr.ar_pro
#define arp_hln ea_hdr.ar_hln
#define arp_pln ea_hdr.ar_pln
#define arp_op ea_hdr.ar_op

struct ether_arp{
	struct arp_hdr ea_hdr;
	u8 arp_sha[ETHER_ADDR_LEN];
	u8 arp_spa[IP_ADDR_LEN];
	u8 arp_tha[ETHER_ADDR_LEN];
	u8 arp_tpa[IP_ADDR_LEN];
};

#define ARPHRD_ETHER 1
#define ETHERTYPE_IP 0x0800

#define ARPOP_REQUEST 1
#define ARPOP_REPLY 2


//ICMP
struct icmp{
	u8 icmp_type;
	u8 icmp_code;
	u16 icmp_cksum;
	union{
		u8 ih_pptr;
		u8 ih_gwaddr[IP_ADDR_LEN];
		struct ih_idseq{
			int16_t icd_id;
			int16_t icd_seq;
		} ih_idseq;
		u32 ih_void;
		struct ih_pmtu{
			u16 ipm_void;
			u16 ipm_nextmtu;
		} ih_pmtu;

		struct ih_rtradv{
			u8 irt_num_addrs;
			u8 irt_wpa;
			u16 irt_lifetime;
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
			u32 its_otime;
			u32 its_rtime;
			u32 its_ttime;
		} id_ts;
		struct id_ip{
			struct ip_hdr idi_ip;
		} id_ip;
		struct icmp_ra_addr{
			u32 ira_addr;
			u32 ira_preference;
		} ip_radv;;
		u32 id_mask;
		u8 id_data[1];
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
	u16 uh_sport;
	u16 uh_dport;
	u16 uh_ulen;
	u16 sum;
};

struct udp_pseudo_hdr{
	u8 up_src[IP_ADDR_LEN];
	u8 up_dst[IP_ADDR_LEN];
	u8 up_void;
	u8 up_type;
	u16 up_len;
};

struct tcp_pseudo_hdr{
	u8 tp_src[IP_ADDR_LEN];
	u8 tp_dst[IP_ADDR_LEN];
	u8 tp_void;
	u8 tp_type;
	u16 tp_len;
};


//TCP
struct tcp_hdr{
	u16 th_sport;
	u16 th_dport;
	u32 th_seq;
	u32 th_ack;
#ifdef BIG_ENDIAN
	unsigned th_off:4,th_x2:4;
#endif
#ifdef LITTLE_ENDIAN
	unsigned th_x2:4,th_off:4;
#endif // BYTE_ORDER
	u8 th_flags;
	u16 th_win;
	u16 th_sum;
	u16 th_urp;
};

#define TH_FIN 0x01
#define TH_SYN 0x02
#define TH_RST 0x04
#define TH_PUSH 0x08
#define TH_ACK 0x10
#define TH_URG 0x20