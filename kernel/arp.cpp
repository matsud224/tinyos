#include "arp.h"
#include "ethernet.h"
#include "util.h"
#include "netconf.h"
#include "protohdr.h"
#include "lock.h"

struct arpentry arptable[MAX_ARPTABLE];
static int next_register = 0; //次の登録位置

enum arpresult {
  RESULT_FOUND			= 0,
  RESULT_NOT_FOUND	= 1,
  RESULT_ADD_LIST		= 2,
};

static mutex arptbl_mtx;

void arp_init() {
  mutex_init(&arptbl_mtx);

	for(int i=0;i<MAX_ARPTABLE;i++)
    list_init(&arptable[i].pending);
}

static void pending_remove_all(struct list_head *pending) {
  struct list_head *p, *tmp;
  list_for_each_safe(p, tmp, pending) {
    struct arpentry *ae = container_of(p, struct arpentry, link);
    list_remove(p);
    free(ae);
  }
}

static int search_arptable(u32 ipaddr, u8 macaddr[], struct pktbuf_head *frm){
	mutex_lock(&arptbl_mtx);

	for(int i=0;i<MAX_ARPTABLE;i++){
		if(arptable[i].ipaddr == ipaddr &&  arptable[i].timeout>0){
			int result;
			if(list_is_empty(arptable[i].pending)){
				//保留無し->アドレス解決できてる
				memcpy(macaddr, arptable[i].macaddr, ETHER_ADDR_LEN);
				result = RESULT_FOUND;
			}else{
				//保留となっているフレームが他にもある
				//受付順に送信するため、末尾に挿入
        list_pushback(&frm->link, &arptable[i].pending);
				result = RESULT_ADD_LIST;
			}
	    mutex_unlock(&arptbl_mtx);
			return result;
		}
	}

	//登録なしなので、IPアドレスだけ登録。保留リストに入れる
	if(!list_is_empty(arptable[next_register].pending)){
		pending_remove_all(&arptable[next_register].pending);
		arptable[next_register].pending = NULL;
	}
	list_pushfront(&frm->link, &arptable[next_register].pending);
	arptable[next_register].timeout=ARBTBL_TIMEOUT_CLC;
	arptable[next_register].ipaddr = ipaddr;
	next_register = (next_register+1) % MAX_ARPTABLE;

	mutex_unlock(&arptbl_mtx);
	return RESULT_NOT_FOUND;
}

void register_arptable(u32 ipaddr, u8 macaddr[], bool is_permanent){
  mutex_lock(&arptbl_mtx);

	//IPアドレスだけ登録されている（アドレス解決待ち）エントリを探す
	for(int i=0;i<MAX_ARPTABLE;i++){
		if(arptable[i].ipaddr == ipaddr && arptable[i].timeout>0){
			arptable[i].timeout=is_permanent?ARPTBL_PERMANENT:ARBTBL_TIMEOUT_CLC; //延長
			if(!list_is_empty(&arptable[i].pending)){
				memcpy(arptable[i].macaddr, macaddr,ETHER_ADDR_LEN);
        struct arpentry *p;
        list_for_each(p, &arptable[i].pending) {
          struct pktbuf_head *frm = container_of(p, struct pktbuf_head, link);
					struct ether_hdr *ehdr = (ether_hdr*)ptr->flm->buf;
					//宛先MACアドレス欄を埋める
					memcpy(ehdr->ether_dhost, macaddr, ETHER_ADDR_LEN);
					ethernet_send(frm);
				}
				pending_remove_all(&arptable[i].pending);
			}
      mutex_unlock(&arptbl_mtx);
			return;
		}
	}
	if(!list_is_empty(&arptable[next_register].pending)){
		pending_remove_all(&arptable[next_register].pending);
	}
	arptable[next_register].timeout=is_permanent?ARPTBL_PERMANENT:ARBTBL_TIMEOUT_CLC;
	arptable[next_register].ipaddr = ipaddr;
	memcpy(arptable[next_register].macaddr, macaddr, ETHER_ADDR_LEN);
	next_register = (next_register+1) % MAX_ARPTABLE;
  mutex_unlock(&arptbl_mtx);
	return;
}

void arp_rx(struct pktbuf_head *frm, struct ether_hdr *ehdr){
	//正しいヘッダかチェック
	if(frm->total < sizeof(ether_arp) ||
		ntoh16(earp->arp_hrd) != ARPHRD_ETHER ||
		ntoh16(earp->arp_pro) != ETHERTYPE_IP ||
		earp->arp_hln != ETHER_ADDR_LEN || earp->arp_pln != 4 ||
		(ntoh16(earp->arp_op) != ARPOP_REQUEST && ntoh16(earp->arp_op) !=ARPOP_REPLY) ){
		goto exit;
	}

	ether_arp *earp = frm->data;

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
	return;
}

struct pktbuf_head *make_arprequest_frame(u8 dstaddr[]){
	struct pktbuf_head *frm =
    pktbuf_alloc(sizeof(struct ether_hdr) + sizeof(struct ether_arp));

  pktbuf_reserve(sizeof(struct ether_hdr) + sizeof(struct ether_arp));
	struct ether_arp *earp =
   (struct ether_arp *) pktbuf_add_header(sizeof(struct ether_arp));
/*
	ehdr->ether_type = hton16(ETHERTYPE_ARP);
	memcpy(ehdr->ether_shost, MACADDR, ETHER_ADDR_LEN);
	memset(ehdr->ether_dhost, 0xff, ETHER_ADDR_LEN); //ブロードキャスト
*/
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

void arp_send(struct pktbuf_head *packet, u8 dstaddr[], u16 proto){
	struct ether_hdr *ehdr =
    (struct ether_hdr *)pktbuf_add_header(sizeof(struct ether_hdr));
	ehdr->ether_type = hton16(proto);
	memcpy(ehdr->ether_shost, MACADDR, ETHER_ADDR_LEN);

	switch(search_arptable(IPADDR_TO_UINT32(dstaddr), ehdr->ether_dhost, packet)){
	case RESULT_FOUND:
		//ARPテーブルに...ある時!
		ethernet_tx(packet);
		break;
	case RESULT_NOT_FOUND:
		//無いとき... はsearch_arptableが保留リストに登録しておいてくれる
		//ARPリクエストを送信する
		{
			struct pktbuf_head *request = make_arprequest_frame(dstaddr);
			ethernet_tx(request, ETHER_ADDR_BROADCAST, ETHERTYPE_ARP);
			break;
		}
	case RESULT_ADD_LIST:
		//以前から保留状態にあった
		break;
	}
}


