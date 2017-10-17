#include "udp.h"
#include "protohdr.h"
#include "sockif.h"
#include "ip.h"
#include "netlib.h"
#include "util.h"
#include "netconf.h"
#include "errnolist.h"

#define MIN(x,y) ((x)<(y)?(x):(y))

struct udp_ctrlblock{
	int recv_front,recv_back;
	ether_flame **recv_queue; //「ether_flameのポインタ」の配列
	//hdrstack **send_queue;
	bool recv_waiting;


	ID ownertsk;
    transport_addr addr;

	~udp_ctrlblock(){
		delete [] recv_queue;
		return;
	}
};

static uint16_t udp_checksum(ip_hdr *iphdr, udp_hdr *uhdr){
	udp_pseudo_hdr pseudo;
	memcpy(pseudo.up_src, iphdr->ip_src, IP_ADDR_LEN);
	memcpy(pseudo.up_dst, iphdr->ip_dst, IP_ADDR_LEN);
	pseudo.up_type = 17;
	pseudo.up_void = 0;
	pseudo.up_len = uhdr->uh_ulen; //UDPヘッダ+UDPペイロードの長さ

	return checksum2((uint16_t*)(&pseudo), (uint16_t*)uhdr, sizeof(udp_pseudo_hdr), ntoh16(uhdr->uh_ulen));
}

void udp_process(ether_flame *flm, ip_hdr *iphdr, udp_hdr *uhdr){
	//ブロードキャスト/マルチキャストアドレスは不許可
	/*if(memcmp(iphdr->ip_dst, IPADDR, IP_ADDR_LEN) != 0){
		goto exit;
	}*/
	//ヘッダ検査
	if(flm->size < sizeof(ether_hdr)+(iphdr->ip_hl*4)+sizeof(udp_hdr) ||
		flm->size != sizeof(ether_hdr)+(iphdr->ip_hl*4)+ntoh16(uhdr->uh_ulen)){
		goto exit;
	}
	if(uhdr->sum != 0 && udp_checksum(iphdr, uhdr) != 0){
		goto exit;
	}

	wai_sem(UDP_SEM);

	int s;
	socket_t *sock;
	for(s=0;s<MAX_SOCKET;s++){
		sock = &sockets[s];
		if(sock->type==SOCK_DGRAM && sock->addr.my_port==ntoh16(uhdr->uh_dport))
			break;
	}

	if(s==MAX_SOCKET){
		goto exit;
	}

	//キューに入れる
	udp_ctrlblock *ucb;
	ucb = sock->ctrlblock.ucb;
	wai_sem(UDP_RECV_SEM);
	ucb->recv_queue[ucb->recv_front] = flm;
	ucb->recv_front++;
	if(ucb->recv_front == DGRAM_RECV_QUEUE) ucb->recv_front=0;
	if(ucb->recv_front == ucb->recv_back){
		//キューがいっぱいなので、古いものから消す
		delete ucb->recv_queue[ucb->recv_front];
		ucb->recv_back++;
		if(ucb->recv_back == DGRAM_RECV_QUEUE) ucb->recv_back=0;
	}

	if(ucb->recv_waiting) wup_tsk(sock->ownertsk);
	sig_sem(UDP_RECV_SEM);

	sig_sem(UDP_SEM);
	return;
exit:
	sig_sem(UDP_SEM);
	delete flm;
	return;
}

//UDPヘッダのチェックサム計算にはIPアドレスが必要
static void set_udpheader(udp_hdr *uhdr, uint16_t seglen, uint16_t sport, uint16_t dport, uint8_t daddr[]){
    uhdr->uh_sport = hton16(sport);
    uhdr->uh_dport = hton16(dport);
    uhdr->uh_ulen = hton16(seglen);
	uhdr->sum = 0;

	//チェックサム計算用(送信元・宛先アドレスだけ埋めればいい)
	ip_hdr iphdr_tmp;
	memcpy(iphdr_tmp.ip_src, IPADDR, IP_ADDR_LEN);
	memcpy(iphdr_tmp.ip_dst, daddr, IP_ADDR_LEN);

    uhdr->sum = udp_checksum(&iphdr_tmp, uhdr);

    return;
}

int udp_sendto(const char *msg, uint32_t len, int flags, uint8_t to_addr[], uint16_t to_port, uint16_t my_port){
	//UDPで送れる最大サイズを超えている
	if(0xffff-sizeof(udp_hdr) < len) return EMSGSIZE;

	hdrstack *udpseg=new hdrstack(true);
	udpseg->next = NULL;
	udpseg->size=sizeof(udp_hdr)+len;
	udpseg->buf=new char[udpseg->size];
	memcpy(udpseg->buf+sizeof(udp_hdr), msg, len);

	set_udpheader((udp_hdr*)udpseg->buf,udpseg->size, my_port, to_port, to_addr);

	ip_send(udpseg, to_addr, IPTYPE_UDP);

	return len;
}

static char *udp_analyze(ether_flame *flm, uint16_t *datalen, uint8_t srcaddr[], uint16_t *srcport){
	ether_hdr *ehdr=(ether_hdr*)(flm->buf);
	ip_hdr *iphdr=(ip_hdr*)(ehdr+1);
	udp_hdr *udphdr=(udp_hdr*)(((uint8_t*)iphdr)+(iphdr->ip_hl*4));
	*datalen = ntoh16(udphdr->uh_ulen)-sizeof(udp_hdr);
	if(srcaddr!=NULL) memcpy(srcaddr, iphdr->ip_src, IP_ADDR_LEN);
	if(srcport!=NULL) *srcport = ntoh16(udphdr->uh_sport);
	return (char*)(udphdr+1);
}

int32_t udp_recvfrom(udp_ctrlblock *ucb, char *buf, uint32_t len, int flags, uint8_t from_addr[], uint16_t *from_port, TMO timeout){
	wai_sem(UDP_RECV_SEM);
	//recv_waitingがtrueの時にデータグラムがやってきたら起こしてもらえる
	ucb->recv_waiting = true;
	while(true){
		if(ucb->recv_front==ucb->recv_back){
			sig_sem(UDP_RECV_SEM);
            if(tslp_tsk(timeout) == E_TMOUT){
				wai_sem(UDP_RECV_SEM);
				ucb->recv_waiting = false;
				sig_sem(UDP_RECV_SEM);
				return ETIMEOUT;
            }
		}else{
			uint16_t datalen;
			char *data = udp_analyze(ucb->recv_queue[ucb->recv_back], &datalen, from_addr, from_port);
			memcpy(buf, data, MIN(len,datalen));
			delete ucb->recv_queue[ucb->recv_back];
			ucb->recv_back++;
			if(ucb->recv_back==DGRAM_RECV_QUEUE) ucb->recv_back=0;
			ucb->recv_waiting = false;
			sig_sem(UDP_RECV_SEM);
			return MIN(len,datalen);
		}

		wai_sem(UDP_RECV_SEM);
	}
}

udp_ctrlblock *ucb_new(ID owner){
	udp_ctrlblock *ucb = new udp_ctrlblock;
	ucb->ownertsk = owner;
	ucb->recv_queue=new ether_flame*[DGRAM_RECV_QUEUE];

	ucb->recv_front=0; ucb->recv_back=0;
	ucb->recv_waiting=false;

	return ucb;
}

void ucb_setaddr(udp_ctrlblock *ucb, transport_addr *addr){
	ucb->addr = *addr;
}

void udp_disposecb(udp_ctrlblock *ucb){
	delete ucb;
	return;
}
