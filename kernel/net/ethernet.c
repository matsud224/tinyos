#include "ethernet.h"
#include "util.h"
#include "protohdr.h"

void etherrecv_task(intptr_t exinf) {
  while(true){
		twai_sem(ETHERRECV_SEM, 10);
		for(int i=0; i<16; i++){
			wai_sem(ETHERIO_SEM);
			u32 size = ethernet_receive();
			if(size > sizeof(ether_hdr)){
				char *buf = new char[size];
				ethernet_read(buf, size);
				sig_sem(ETHERIO_SEM);

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

void ethernet_send(ether_flame *flm){
	ethernet_write(flm->buf, flm->size);
	ethernet_send();
	sig_sem(ETHERIO_SEM);
	return;
}

