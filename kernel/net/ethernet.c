#include "ethernet.h"
#include "util.h"
#include "protohdr.h"

static void ethrcv_callback(){
    isig_sem(ETHERRECV_SEM);
}

void ethernet_initialize(){
	//lwipドライバ(rza1_emac.c)を参考にした
	ethernet_cfg_t ethcfg;

	ethcfg.int_priority = 6;
	ethcfg.recv_cb = &ethrcv_callback;
	ethcfg.ether_mac = NULL;
	ethernetext_init(&ethcfg);
	ethernet_set_link(-1,0);

	ethernet_address((char *)MACADDR);

}

static int recvskip_counter = 0;

void etherrecv_task(intptr_t exinf) {
	wait(1);
  while(true){
		twai_sem(ETHERRECV_SEM, 10);
		for(int i=0; i<16; i++){
			wai_sem(ETHERIO_SEM);
			uint32_t size = ethernet_receive();
			if(size > sizeof(ether_hdr)){
				char *buf = new char[size];
				ethernet_read(buf, size);
				sig_sem(ETHERIO_SEM);

				if(ETHER_RECV_SKIP >= 0 && recvskip_counter >= ETHER_RECV_SKIP){
					recvskip_counter = 0;
					delete [] buf;
					continue;
				}
				recvskip_counter++;

				ether_hdr *ehdr = (ether_hdr*)buf;
				if(memcmp(ehdr->ether_dhost, MACADDR, ETHER_ADDR_LEN)!=0 &&
					 memcmp(ehdr->ether_dhost, ETHERBROADCAST, ETHER_ADDR_LEN)!=0){
					delete [] buf; continue;
				}

				ether_flame *flm = new ether_flame;
				flm->size = size;
				flm->buf = buf;
				switch(ntoh16(ehdr->ether_type)){
				case ETHERTYPE_IP:
					ip_process(flm, (ip_hdr*)(ehdr+1));
					break;
				case ETHERTYPE_ARP:
					arp_process(flm, (ether_arp*)(ehdr+1));
					break;
				default:
					delete flm;
					break;
				}
			}else{
				sig_sem(ETHERIO_SEM);
			}
		}
    }
}

static int sendskip_counter = 0;

void ethernet_send(ether_flame *flm){
	wai_sem(ETHERIO_SEM);

	if(ETHER_SEND_SKIP >= 0 && sendskip_counter >= ETHER_SEND_SKIP){
		sendskip_counter = 0;
		sig_sem(ETHERIO_SEM);
		return;
	}
	sendskip_counter++;

	ethernet_write(flm->buf, flm->size);
	ethernet_send();
	sig_sem(ETHERIO_SEM);
	return;
}

void ethernet_send(hdrstack *flm){
	wai_sem(ETHERIO_SEM);

	if(ETHER_SEND_SKIP >= 0 && sendskip_counter >= ETHER_SEND_SKIP){
		sendskip_counter = 0;
		sig_sem(ETHERIO_SEM);
		return;
	}
	sendskip_counter++;

	while(flm!=NULL){
		ethernet_write(flm->buf, flm->size);
		flm=flm->next;
	}
	ethernet_send();
	sig_sem(ETHERIO_SEM);
	return;
}
