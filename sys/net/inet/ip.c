#include <net/inet/ip.h>
#include <net/inet/arp.h>
#include <net/inet/icmp.h>
#include <net/inet/util.h>
#include <net/inet/params.h>
#include <net/inet/protohdr.h>

#define INF 0xffff

struct hole{
	struct hole *next;
	u16 first;
	u16 last;
};

struct fragment{
	struct fragment *next;
	u16 first;
	u16 last;
	struct pktbuf_head *frm;
};

static void fragment_free(struct fragment *frag) {
  struct fragment *cur = frag;
  struct fragment *next = NULL;
  while(cur != NULL) {
    next = cur->next;
    pktbuf_free(cur->frm);
    free(cur);
    cur = next;
  }
}

struct reasminfo{
	struct reasminfo *next;
	struct{
		in_addr_t ip_src;
		in_addr_t ip_dst;
		u8 ip_pro;
		u16 ip_id;
	} id;
	struct hole *holelist;
	struct fragment *fragmentlist;
	struct pktbuf_head *beginningframe;
	u8 headerlen; //etherヘッダ込
	u16 datalen;
	int16_t timeout; //タイムアウトまでのカウント
};

static void reasminfo_free(struct reasminfo *ri) {
  struct hole *hptr = holelist;
  while(hptr!=NULL){
    struct hole *tmp = hptr;
    hptr=hptr->next;
    free(tmp);
  }
  fragment_free(fragmentlist);
}

static struct reasminfo *reasm_ongoing = NULL;

void start_ip(){
	act_tsk(ETHERRECV_TASK);
	act_tsk(TIMEOUT_10SEC_TASK);
	sta_cyc(TIMEOUT_10SEC_CYC);
}

void ip_10sec() {
	//10sec周期で動き、タイムアウトを管理するタスク
	while(true){
		//IPフラグメント組み立てタイムアウト
		wai_sem(TIMEOUT_10SEC_SEM);
		struct reasminfo **pp = &reasm_ongoing;
		while(*pp!=NULL){
			if(--((*pp)->timeout) <= 0 || (*pp)->holelist==NULL){
				reasminfo *tmp=*pp;
				*pp=(*pp)->next;
				delete tmp;
			}
			pp=&((*pp)->next);
		}
		sig_sem(TIMEOUT_10SEC_SEM);

		//ARPテーブル
		wai_sem(ARPTBL_SEM);
		for(int i=0; i<MAX_ARPTABLE; i++){
			if(arptable[i].timeout>0 && arptable[i].timeout!=ARPTBL_PERMANENT){
				arptable[i].timeout--;
			}
			if(arptable[i].timeout == 0){
				if(arptable[i].pending!=NULL){
					delete arptable[i].pending;
					arptable[i].pending = NULL;
				}
			}else{
				if(arptable[i].pending!=NULL){
					ether_flame *request = make_arprequest_flame((u8*)(&arptable[i].ipaddr));
					ethernet_send(request);
					delete request;
				}
			}
		}
		sig_sem(ARPTBL_SEM);
	}
  
  defer_exec(ip_10sec, 
}

static reasminfo *get_reasminfo(in_addr_t ip_src, in_addr_t ip_dst, u8 ip_pro, u16 ip_id){
	// already locked.
	struct reasminfo *ptr = reasm_ongoing;
	while(ptr!=NULL){
		if(memcmp(ptr->id.ip_src,ip_src,IP_ADDR_LEN)==0 &&
			memcmp(ptr->id.ip_dst,ip_dst,IP_ADDR_LEN)==0 &&
			ptr->id.ip_pro==ip_pro && ptr->id.ip_id==ip_id &&
			ptr->timeout > 0){
			return ptr;
		}
	}
	struct reasminfo *info = malloc(sizeof(struct reasminfo));
	info->id.ip_src = ip_src;
	info->id.ip_dst = ip_dst;
	info->id.ip_pro=ip_pro; info->id.ip_id=ip_id;
	info->timeout = IPFRAG_TIMEOUT_CLC;
	info->next = reasm_ongoing;
	struct hole *newh = malloc(sizeof(struct hole));
	newh->first=0;
	newh->last=INF;
	newh->next=NULL;
	info->holelist = newh;
	info->fragmentlist=NULL;
	info->beginningflame=NULL;
	reasm_ongoing = info;
	return info;
}

static void show_holelist(struct hole *holelist){
	puts("---");
	while(holelist!=NULL){
		puts("  %d ~ %d",holelist->first,holelist->last);
		holelist=holelist->next;
	}
	puts("---");
}

//データ領域のサイズが分かったので、last=無限大(0xffff)のホールを修正
static void modify_inf_holelist(struct hole **holepp, u16 newsize){
	for(;*holepp!=NULL;holepp = &((*holepp)->next)){
		if((*holepp)->last==INF){
			(*holepp)->last=newsize;
			if((*holepp)->first==(*holepp)->last){
				struct hole *tmp=*holepp;
				*holepp = (*holepp)->next;
				delete tmp;
			}
			return;
		}
	}
}

void ip_rx(struct pktbuf_head *pkt){
  struct ip_hdr *iphdr = pkt->data;
	//正しいヘッダかチェック
	if(pkt->total < sizeof(struct ip_hdr))
		goto exit;
	if(pkt->total < ntoh16(iphdr->ip_len))
		goto exit;

	if(iphdr->ip_v != 4)
		goto exit;
	if(iphdr->ip_hl < 5){
		goto exit;
	if(checksum((u16*)iphdr, iphdr->ip_hl*4) != 0)
		goto exit;

	//自分宛てかチェック
	/*
	if(memcmp(iphdr->ip_dst, IPADDR, IP_ADDR_LEN) != 0){
		u32 addr=ntoh32(*(u32*)IPADDR),
					mask=ntoh32(*(u32*)NETMASK),broad;
		broad = hton32((addr & mask) | (~(mask)));
		if(memcmp(iphdr->ip_dst, &broad, IP_ADDR_LEN) != 0){
			goto exit;
		}
	}
	*/

	if(!((ntoh16(iphdr->ip_off) & IP_OFFMASK) == 0 && (ntoh16(iphdr->ip_off) & IP_MF) == 0)){
		//フラグメント
		wai_sem(TIMEOUT_10SEC_SEM);
		u16 ffirst = (ntoh16(iphdr->ip_off) & IP_OFFMASK)*8;
		u16 flast = ffirst + (ntoh16(iphdr->ip_len) - iphdr->ip_hl*4) - 1;

		struct reasminfo *info = get_reasminfo(iphdr->ip_src,iphdr->ip_dst,iphdr->ip_p,ntoh16(iphdr->ip_id));
		for(struct hole **holepp = &(info->holelist);*holepp!=NULL;){
			u16 hfirst=(*holepp)->first;
			u16 hlast=(*holepp)->last;
			//現holeにかぶっているか
			if(ffirst>hlast || flast<hfirst){
				holepp = &((*holepp)->next);
				continue;
			}
			//holeを削除
			struct hole *tmp=*holepp;
			*holepp = (*holepp)->next;
			free(tmp);
			//タイムアウトを延長
			info->timeout=IPFRAG_TIMEOUT_CLC;
			//現holeにかぶっているか
			if(ffirst>hfirst){
				//holeを追加
				struct hole *newh = malloc(sizeof(struct hole));
				newh->first=hfirst;
				newh->last=ffirst-1;
				newh->next=*holepp;
				*holepp=newh;
				holepp=&(newh->next);
			}
			if(flast<hlast){
				//holeを追加
				struct hole *newh = malloc(sizeof(struct hole));
				newh->first=flast+1;
				newh->last=hlast;
				newh->next=*holepp;
				*holepp=newh;
				holepp=&(newh->next);
			}
			//fragmentを追加
			struct fragment *newf = malloc(sizeof(struct fragment));
			newf->next=info->fragmentlist;
			info->fragmentlist=newf;
			newf->first=ffirst; newf->last=flast;
			newf->frm = frm;
			//more fragment がOFF
			if((ntoh16(iphdr->ip_off) & IP_MF) == 0){
				info->datalen=flast+1;
				modify_inf_holelist(&(info->holelist),info->datalen);
			}
			//はじまりのフラグメント
			if(ffirst==0){
				info->beginningflame = frm;
				info->headerlen=sizeof(ether_hdr)+iphdr->ip_hl*4;
			}

		}
		if(info->holelist==NULL){
			//holeがなくなり、おしまい
			//パケットを構築
			frm = pktbuf_alloc(info->headerlen+info->datalen);
			//LOG("total %d/%d",info->headerlen,info->datalen);
			memcpy(frm->buf,info->beginningframe->buf,info->headerlen);
			char *origin = frm->buf + info->headerlen;
			int total=0;
      for(struct fragment *fptr=info->fragmentlist;fptr!=NULL;fptr=fptr->next){
        memcpy(origin+fptr->first,((u8*)(fptr->frm->buf))+info->headerlen,fptr->last-fptr->first+1);
        total+=fptr->last-fptr->first+1;
      }
      //frmを上書きしたので、iphdrも修正必要
      iphdr = (ip_hdr*)(frm->buf+sizeof(ether_hdr));
      info->timeout=0;
		}else{
			sig_sem(TIMEOUT_10SEC_SEM);
			return;
		}
		sig_sem(TIMEOUT_10SEC_SEM);
	}

	switch(iphdr->ip_p){
	case IPTYPE_ICMP:
		icmp_rx(pkt, iphdr);
		break;
	case IPTYPE_TCP:
		//tcp_rx(frm, iphdr, (tcp_hdr*)(((u8*)iphdr)+(iphdr->ip_hl*4)));
		break;
	case IPTYPE_UDP:
		udp_rx(frm, iphdr);
		break;
	}

	return;
}

static u16 ip_id = 0;

static void prep_iphdr(ip_hdr *iphdr, u16 len, u16 id,
					bool mf, u16 offset, u8 proto, in_addr_t dstaddr){
  iphdr->ip_v = 4;
  iphdr->ip_hl = sizeof(ip_hdr)/4;
  iphdr->ip_tos = 0x80;
  iphdr->ip_len = hton16(len);
  iphdr->ip_id = hton16(id);
  iphdr->ip_off = hton16((offset/8) | (mf?IP_MF:0));
  iphdr->ip_ttl = IP_TTL;
  iphdr->ip_p = proto;
  iphdr->ip_sum = 0;
  iphdr->ip_src =  IPADDR;
  iphdr->ip_dst =  dstaddr;

  iphdr->ip_sum = checksum((u16*)iphdr, sizeof(ip_hdr));
  return;
}

in_addr_t ip_routing(in_addr_t dstaddr){
	in_addr_t myaddr=IPADDR_TO_UINT32(IPADDR);
	in_addr_t mymask=IPADDR_TO_UINT32(NETMASK);
	in_addr_t dst = IPADDR_TO_UINT32(dstaddr);
	if(dst!=0 && (myaddr&mymask)!=(dst&mymask) && dstaddr != IPBROADCAST){
		//同一のネットワークでない->デフォルトゲートウェイに流す
		dstaddr = DEFAULT_GATEWAY;
	}

  return dstaddr;
}

u16 ip_getid(){
	u16 id;
	wai_sem(IPID_SEM);
  id = ip_id; ip_id++;
  sig_sem(IPID_SEM);
  return id;
}

void ip_tx(struct pktbuf_head *data, in_addr_t dstaddr, u8 proto){
	u32 datalen = data->total; //IPペイロード長
	u32 remainlen = datalen;
  u16 currentid = ip_getid(); //今回のパケットに付与する識別子

  //ルーティング
  in_addr_t dstaddr_r;
  dstaddr_r =  dstaddr; //ip_routing()で書き換えられるかもしれないのでコピー
	ip_routing(dstaddr_r);

  //複数のフラグメントを送信する際、iphdr_itemはつなぐ先と内容を変えながら使い回せそうに思える
  //でも、送信はすぐに行われないかもしれない(MACアドレス解決待ち)ので使い回しはダメ
  if(sizeof(ip_hdr)+remainlen <= MTU){
		hdrstack *iphdr_item = new hdrstack(true);
		iphdr_item->size = sizeof(ip_hdr);
		iphdr_item->buf = new char[sizeof(ip_hdr)];
		prep_iphdr((ip_hdr*)iphdr_item->buf, sizeof(ip_hdr)+remainlen, currentid, false, 0, proto, dstaddr);
		iphdr_item->next = data;
		arp_send(iphdr_item, dstaddr_r, ETHERTYPE_IP);
  }else{
    //フラグメント化必要
		//フラグメント化に際して、IPペイロードを分割後のパケットにコピーしないといけない
		while(remainlen > 0){
			u32 thispkt_totallen = MIN(remainlen+sizeof(ip_hdr), MTU);
			u32 thispkt_datalen = thispkt_totallen - sizeof(ip_hdr);
			u16 offset = datalen - remainlen;
			hdrstack *ippkt = new hdrstack(true);
			ippkt->next = NULL;
			remainlen -= thispkt_datalen;
			ippkt->size = thispkt_totallen;
			ippkt->buf = new char[ippkt->size];
			prep_iphdr((ip_hdr*)ippkt->buf, thispkt_totallen, currentid, (remainlen>0)?true:false
						, offset, proto, dstaddr);
			hdrstack_cpy((char*)(((ip_hdr*)ippkt->buf)+1), data, offset, thispkt_datalen);

			arp_send(ippkt, dstaddr_r, ETHERTYPE_IP);
		}
  }
}
