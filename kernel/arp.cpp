#include "arp.h"
#include "ethernet.h"
#include "util.h"
#include "netconf.h"
#include "protohdr.h"

struct arpentry arptable[MAX_ARPTABLE];
static int next_register = 0; //次の登録位置

enum arpresult {
  RESULT_FOUND			= 0,
  RESULT_NOT_FOUND	= 1,
  RESULT_ADD_LIST		= 2,
};

static int search_arptable(uint32_t ipaddr, uint8_t macaddr[], struct pktbuf_head *frm){
	wai_sem(ARPTBL_SEM);

	for(int i=0;i<MAX_ARPTABLE;i++){
		if(arptable[i].ipaddr == ipaddr &&  arptable[i].timeout>0){
			int result;
			if(arptable[i].pending==NULL){
				//保留無し->アドレス解決できてる
				memcpy(macaddr, arptable[i].macaddr, ETHER_ADDR_LEN);
				result = RESULT_FOUND;
			}else{
				//保留となっているフレームが他にもある
				flist *pf = new flist;
				pf->flm = flm; pf->next = NULL;
				flist *ptr = arptable[i].pending;
				//受付順に送信するため、末尾に挿入
				while(ptr->next!=NULL)
					ptr=ptr->next;
				ptr->next = pf;
				result = RESULT_ADD_LIST;
			}
			sig_sem(ARPTBL_SEM);
			return result;
		}
	}

	//登録なしなので、IPアドレスだけ登録。保留リストに入れる
	if(arptable[next_register].pending!=NULL){
		delete arptable[next_register].pending;
		arptable[next_register].pending = NULL;
	}
	flist *pf = new flist;
	pf->flm = flm; pf->next = NULL;
	arptable[next_register].pending = pf;
	arptable[next_register].timeout=ARBTBL_TIMEOUT_CLC;
	arptable[next_register].ipaddr = ipaddr;
	next_register = (next_register+1) % MAX_ARPTABLE;
	sig_sem(ARPTBL_SEM);
	return RESULT_NOT_FOUND;
}

void register_arptable(uint32_t ipaddr, uint8_t macaddr[], bool is_permanent){
	wai_sem(ARPTBL_SEM);

	//IPアドレスだけ登録されている（アドレス解決待ち）エントリを探す
	for(int i=0;i<MAX_ARPTABLE;i++){
		if(arptable[i].ipaddr == ipaddr && arptable[i].timeout>0){
			arptable[i].timeout=is_permanent?ARPTBL_PERMANENT:ARBTBL_TIMEOUT_CLC; //延長
			if(arptable[i].pending!=NULL){
				memcpy(arptable[i].macaddr, macaddr,ETHER_ADDR_LEN);
				flist *ptr = arptable[i].pending;
				while(ptr!=NULL){
					ether_hdr *ehdr = (ether_hdr*)ptr->flm->buf;
					//宛先MACアドレス欄を埋める
					memcpy(ehdr->ether_dhost, macaddr, ETHER_ADDR_LEN);
					ethernet_send(ptr->flm);
					ptr=ptr->next;
				}

				delete arptable[i].pending;
				arptable[i].pending = NULL;
			}
			sig_sem(ARPTBL_SEM);
			return;
		}
	}
	if(arptable[next_register].pending!=NULL){
		delete arptable[next_register].pending;
		arptable[next_register].pending = NULL;
	}
	arptable[next_register].timeout=is_permanent?ARPTBL_PERMANENT:ARBTBL_TIMEOUT_CLC;
	arptable[next_register].ipaddr = ipaddr;
	memcpy(arptable[next_register].macaddr, macaddr, ETHER_ADDR_LEN);
	next_register = (next_register+1) % MAX_ARPTABLE;
	sig_sem(ARPTBL_SEM);
	return;
}

void arp_rx(struct pktbuf_head *frm, struct ether_arp *earp){
	ether_hdr *ehdr = (ether_hdr*)flm->buf;
	//正しいヘッダかチェック
	if(flm->size < sizeof(ether_hdr)+sizeof(ether_arp) ||
		ntoh16(earp->arp_hrd) != ARPHRD_ETHER ||
		ntoh16(earp->arp_pro) != ETHERTYPE_IP ||
		earp->arp_hln != ETHER_ADDR_LEN || earp->arp_pln != 4 ||
		(ntoh16(earp->arp_op) != ARPOP_REQUEST && ntoh16(earp->arp_op) !=ARPOP_REPLY) ){
		goto exit;
	}

	switch(ntoh16(earp->arp_op)){
	case ARPOP_REQUEST:
		if(memcmp(earp->arp_tpa, IPADDR,IP_ADDR_LEN)==0){
			//相手のIPアドレスとMACアドレスを登録
			register_arptable(IPADDR_TO_UINT32(earp->arp_spa), earp->arp_sha, false);

			//パケットを改変
			memcpy(earp->arp_tha, earp->arp_sha, ETHER_ADDR_LEN);
			memcpy(earp->arp_tpa, earp->arp_spa, IP_ADDR_LEN);
			memcpy(earp->arp_sha, MACADDR, ETHER_ADDR_LEN);
			memcpy(earp->arp_spa, IPADDR, IP_ADDR_LEN);
            earp->arp_op = hton16(ARPOP_REPLY);
            memcpy(ehdr->ether_dhost, ehdr->ether_shost, ETHER_ADDR_LEN);
            memcpy(ehdr->ether_shost, MACADDR, ETHER_ADDR_LEN);
            //送り返す
			ethernet_send(flm);
		}
		break;
	case ARPOP_REPLY:
		register_arptable(IPADDR_TO_UINT32(earp->arp_spa), earp->arp_sha, false);
		break;
	}

exit:
	delete flm;
	return;
}

struct pktbuf_head *make_arprequest_frame(uint8_t dstaddr[]){
	struct pktbuf_head *frm =
    pktbuf_alloc(sizeof(struct ether_hdr) + sizeof(struct ether_arp));
  pktbuf_reserve(sizeof(struct ether_hdr) + sizeof(struct ether_arp));
	struct ether_hdr *ehdr = 
    (struct ether_hdr *)pktbuf_add_header(sizeof(struct ether_hdr));
	struct ether_arp *earp =
   (struct ether_arp *) pktbuf_add_header(sizeof(struct ether_arp));

	ehdr->ether_type = hton16(ETHERTYPE_ARP);
	memcpy(ehdr->ether_shost, MACADDR, ETHER_ADDR_LEN);
	memset(ehdr->ether_dhost, 0xff, ETHER_ADDR_LEN); //ブロードキャスト

	earp->arp_hrd = hton16(ARPHRD_ETHER);
	earp->arp_pro = hton16(ETHERTYPE_IP);
	earp->arp_hln = 6;
	earp->arp_pln = 4;
	earp->arp_op = hton16(ARPOP_REQUEST);
	memcpy(earp->arp_sha, MACADDR, ETHER_ADDR_LEN);
	memcpy(earp->arp_spa, IPADDR, IP_ADDR_LEN);
	memset(earp->arp_tha, 0x00, ETHER_ADDR_LEN);
	memcpy(earp->arp_tpa, dstaddr, IP_ADDR_LEN);
	return frm;
}

void arp_send(struct pktbuf_head *packet, uint8_t dstaddr[], uint16_t proto){
	ether_hdr *ehdr =
    (struct ether_hdr *)pktbuf_add_header(sizeof(struct ether_hdr));
	ehdr->ether_type = hton16(proto);
	memcpy(ehdr->ether_shost, MACADDR, ETHER_ADDR_LEN);

	switch(search_arptable(IPADDR_TO_UINT32(dstaddr), ehdr->ether_dhost, ehdr_item)){
	case RESULT_FOUND:
		//ARPテーブルに...ある時!
		ethernet_send(packet);
		break;
	case RESULT_NOT_FOUND:
		//無いとき... はsearch_arptableが保留リストに登録しておいてくれる
		//ARPリクエストを送信する
		{
			struct pktbuf_head *request = make_arprequest_frame(dstaddr);
			ethernet_send(request);
			break;
		}
	case RESULT_ADD_LIST:
		//以前から保留状態にあった
		break;
	}
}


