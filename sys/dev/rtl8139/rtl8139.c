#include <kern/netdev.h>
#include <kern/idt.h>
#include <kern/pic.h>
#include <kern/kernlib.h>
#include <kern/pci.h>
#include <kern/params.h>
#include <net/ether/ether.h>
#include <net/inet/inet.h>
#include <kern/thread.h>
#include <kern/workqueue.h>
#include <kern/lock.h>

#define RTL8139_VENDORID 0x10ec
#define RTL8139_DEVICEID 0x8139

enum regs {
  IDR				= 0x00,
  MAR				= 0x08,
  TSD				= 0x10,
  TSAD			= 0x20,
  RBSTART		= 0x30,
  ERBCR			= 0x34,
  ERSR			= 0x36,
  CR				= 0x37,
  CAPR			= 0x38,
  CBR				= 0x3a,
  IMR				= 0x3c,
  ISR				= 0x3e,
  TCR				= 0x40,
  RCR				= 0x44,
  TCTR			= 0x48,
  MPC				= 0x4c,
  CR9346		= 0x50,
  CONFIG0		= 0x51,
  CONFIG1		= 0x52,
};

enum cr {
  CR_BUFE		= 0x1,
  CR_TE			= 0x4,
  CR_RE			= 0x8,
  CR_RST		= 0x10,
};

enum rcr {
  RCR_AAP					= 0x1,
  RCR_APM					= 0x2,
  RCR_AM					= 0x4,
  RCR_AR					= 0x8,
  RCR_WRAP				= 0x80,
  RCR_ALL_ACCEPT	= (RCR_AAP|RCR_APM|RCR_AM|RCR_AR),
};

enum pkthdr {
  PKTHDR_ROK	= 0x1,
  PKTHDR_FAE	= 0x2,
  PKTHDR_CRC	= 0x4,
  PKTHDR_LONG	= 0x8,
  PKTHDR_RUNT	= 0x10,
  PKTHDR_ISE	= 0x20,
  PKTHDR_BAR	= 0x2000,
  PKTHDR_PAM	= 0x4000,
  PKTHDR_MAR	= 0x8000,
};

enum isr {
  ISR_ROK			= 0x1,
  ISR_RER			= 0x2,
  ISR_TOK			= 0x4,
  ISR_TER			= 0x8,
  ISR_RXOVW		= 0x10,
  ISR_PUN			= 0x20,
  ISR_FOVW		= 0x40,
  ISR_LENCHG	= 0x2000,
  ISR_TIMEOUT	= 0x4000,
  ISR_SERR		= 0x8000,
};

enum tsd {
  TSD_OWN			= 0x2000,
  TSD_TUN			= 0x4000,
  TSD_TOK			= 0x8000,
  TSD_CDH			= 0x10000000,
  TSD_OWC			= 0x20000000,
  TSD_TABT		= 0x40000000,
  TSD_CRS			= 0x80000000,
};

#define RXBUF_SIZE			8192
#define RXBUF_PAD				16
#define RXBUF_WRAP_PAD	2048
#define RXBUF_TOTAL			(RXBUF_SIZE+RXBUF_PAD+RXBUF_WRAP_PAD)

#define RXQUEUE_COUNT		64
#define TXQUEUE_COUNT		64

#define TXDESC_NUM				4

static struct {
  struct pci_dev *pci;
  u16 iobase;
  u8 irq;
  u8 macaddr[6];
  u8 rxbuf[RXBUF_TOTAL];
  u16 rxbuf_index;
  u32 rxbuf_size;
  u8 txdesc_head;
  u8 txdesc_tail;
  u8 txdesc_free;
  struct pktbuf *txdesc_pkt[TXDESC_NUM];
	struct queue_head rxqueue;
	struct queue_head txqueue;
  mutex rxqueue_mtx;
  mutex txqueue_mtx;
  struct netdev netdev_info;
} rtldev;

#define RTLREG(r) ((r)+rtldev.iobase)

static const u16 TX_TSD[TXDESC_NUM] = {
  TSD, TSD+4, TSD+8, TSD+12
};

static const u16 TX_TSAD[TXDESC_NUM] = {
  TSAD, TSAD+4, TSAD+8, TSAD+12
};

void rtl8139_open(struct netdev *dev);
void rtl8139_close(struct netdev *dev);
int rtl8139_tx(struct netdev *dev, struct pktbuf *pkt);
struct pktbuf *rtl8139_rx(struct netdev *dev);
static const struct netdev_ops rtl8139_ops = {
  .open = rtl8139_open,
  .close = rtl8139_close,
  .tx = rtl8139_tx,
  .rx = rtl8139_rx
};

static struct workqueue *rx_wq;
static struct workqueue *tx_wq;

void rtl8139_isr(void);
void rtl8139_inthandler(void);

void rtl8139_init(struct pci_dev *thisdev) {
  rx_wq = workqueue_new("rtl8139_rx_wq");
  tx_wq = workqueue_new("rtl8139_tx_wq");

  rtldev.netdev_info.ops = &rtl8139_ops;
  rtldev.pci = thisdev;
  rtldev.iobase = pci_config_read32(thisdev, PCI_BAR0);
  rtldev.iobase &= 0xfffc;
  rtldev.irq = pci_config_read8(thisdev, PCI_INTLINE);
  rtldev.rxbuf_index = 0;
  rtldev.txdesc_head = rtldev.txdesc_tail = 0;
  rtldev.txdesc_free = TXDESC_NUM;
	queue_init(&rtldev.rxqueue, RXQUEUE_COUNT);
	queue_init(&rtldev.txqueue, TXQUEUE_COUNT);
	mutex_init(&rtldev.rxqueue_mtx);
	mutex_init(&rtldev.txqueue_mtx);

	list_init(&rtldev.netdev_info.ifaddr_list);

  struct ifaddr *eaddr = malloc(sizeof(struct ifaddr)+ETHER_ADDR_LEN);
  eaddr->len = ETHER_ADDR_LEN;
  eaddr->family = PF_LINK;
  for(int i=0; i<ETHER_ADDR_LEN; i++)
    eaddr->addr[i] = in8(RTLREG(IDR)+i);
  netdev_add_ifaddr(&rtldev.netdev_info, eaddr);

  struct ifaddr_in *inaddr = malloc(sizeof(struct ifaddr_in));
  inaddr->len = sizeof(in_addr_t) * 3;
  inaddr->family = PF_INET;
  inaddr->addr = IPADDR(192,168,4,2);
  inaddr->netmask = IPADDR(255,255,255,0);
  netdev_add_ifaddr(&rtldev.netdev_info, inaddr);

  //enable PCI bus mastering
  u16 pci_cmd = pci_config_read16(thisdev, PCI_COMMAND);
  pci_cmd |= 0x4;
  //pci_config_write16(thisdev, PCI_COMMAND, pci_cmd);

/* FIXME: pci_config_write8 & 16 are  broken. */

#define PCI_CONFIG_ADDR 0xcf8
#define PCI_CONFIG_DATA 0xcfc
  u32 addr = (thisdev->bus<<16) | (thisdev->dev<<11) | (thisdev->func<<8) | (PCI_COMMAND) | 0x80000000u;
  out32(PCI_CONFIG_ADDR, addr);
  out16(PCI_CONFIG_DATA, pci_cmd);

  //printf("pcicmd=%x\n", pci_config_read16(thisdev, PCI_COMMAND));

  //power on
  out8(RTLREG(CONFIG1), 0x0);
  //software reset
  out8(RTLREG(CR), CR_RST);
  while((in8(RTLREG(CR))&CR_RST) != 0);
  //set rx buffer address
  out32(RTLREG(RBSTART), KERN_VMEM_TO_PHYS(rtldev.rxbuf));
  //set IMR
  out16(RTLREG(IMR), 0x55);
  //receive configuration
  out32(RTLREG(RCR), RCR_ALL_ACCEPT | RCR_WRAP);
  //enable rx&tx
  out8(RTLREG(CR), CR_RE|CR_TE);
  //setup interrupt
  idt_register(rtldev.irq+0x20, IDT_INTGATE, rtl8139_inthandler); 
  pic_clearmask(rtldev.irq);
}

DRIVER_INIT int rtl8139_probe() {
  struct pci_dev *thisdev = pci_search_device(RTL8139_VENDORID, RTL8139_DEVICEID);
  if(thisdev != NULL) {
    rtl8139_init(thisdev);
    rtldev.netdev_info.ops = &rtl8139_ops;
    netdev_add(&rtldev.netdev_info);
  }
  return (thisdev != NULL);
}

int rtl8139_tx_one() {
  int error = 0;
  mutex_lock(&rtldev.txqueue_mtx);
IRQ_DISABLE
  if(rtldev.txdesc_free > 0) {
    struct pktbuf *pkt = NULL;
	  if(queue_is_empty(&rtldev.txqueue)) {
      error = -1;
      goto out;
    }


    pkt = list_entry(queue_dequeue(&rtldev.txqueue), struct pktbuf, link);

    out32(RTLREG(TX_TSAD[rtldev.txdesc_head]), KERN_VMEM_TO_PHYS(pkt->head));
    rtldev.txdesc_pkt[rtldev.txdesc_head] = pkt;
    out32(RTLREG(TX_TSD[rtldev.txdesc_head]), pktbuf_get_size(pkt)); 
    rtldev.txdesc_free--;
    rtldev.txdesc_head = (rtldev.txdesc_head+1) % TXDESC_NUM;
  }else {
    error = -2;
  }
out:
IRQ_RESTORE
  mutex_unlock(&rtldev.txqueue_mtx);
  return error;
}

void rtl8139_tx_all(void *arg) {
  while(rtl8139_tx_one() == 0);
  thread_wakeup(&rtldev.netdev_info);
}

int rtl8139_rx_one() {
  int error = 0;
  mutex_lock(&rtldev.rxqueue_mtx);
IRQ_DISABLE
  if((in8(RTLREG(CR)) & CR_BUFE) == 1) {
    error = -1;
    goto err;
  }

  u32 offset = rtldev.rxbuf_index % RXBUF_SIZE;
  u32 pkt_hdr = *((u32 *)(rtldev.rxbuf+offset));
  u16 rx_status = pkt_hdr&0xffff;
  u16 rx_size = pkt_hdr>>16;

  if(rx_status & PKTHDR_RUNT ||
     rx_status & PKTHDR_LONG ||
     rx_status & PKTHDR_CRC ||
     rx_status & PKTHDR_FAE ||
     (rx_status & PKTHDR_ROK) == 0) {
    puts("bad packet.");
    error = -2;
    goto out;
  }

	if(!queue_is_full(&rtldev.rxqueue)) {
    u8 *buf = malloc(rx_size-4);
    memcpy(buf, rtldev.rxbuf+offset+4, rx_size-4);
    struct pktbuf *pkt = pktbuf_create(buf, rx_size-4, free);
    queue_enqueue(&pkt->link, &rtldev.rxqueue);
    //printf("received %dbytes\n", rx_size-4);
	} else {
    //printf("dropped %dbytes\n", rx_size-4);
    error = -3;
  }
  
out:
  rtldev.rxbuf_index = (offset + rx_size + 4 + 3) & ~3;
  out16(RTLREG(CAPR), rtldev.rxbuf_index - 16);
err:
IRQ_RESTORE
  mutex_unlock(&rtldev.rxqueue_mtx);
  return error;
}

void rtl8139_rx_all(void *arg) {
  int rx_count = 0;
  while(rtl8139_rx_one() == 0)
    rx_count++;

  if(rx_count > 0) {
    thread_wakeup(&rtldev.netdev_info);
    workqueue_add(ether_wq, ether_rx, &rtldev.netdev_info);
  }
}

void rtl8139_isr() {
  u16 isr = in16(RTLREG(ISR));

  while(rtldev.txdesc_free < 4 &&
    (in32(RTLREG(TX_TSD[rtldev.txdesc_tail]))&(TSD_OWN|TSD_TOK)) == (TSD_OWN|TSD_TOK)) {
    pktbuf_free(rtldev.txdesc_pkt[rtldev.txdesc_tail]);
    rtldev.txdesc_tail = (rtldev.txdesc_tail+1) % TXDESC_NUM;
    rtldev.txdesc_free++;
  }

  if(isr & ISR_TOK)
    workqueue_add(tx_wq, rtl8139_tx_all, NULL);
  
  if(isr & ISR_ROK)
    workqueue_add(rx_wq, rtl8139_rx_all, NULL);

  if(isr & ISR_TOK)
    out16(RTLREG(ISR), ISR_TOK);
  if(isr & (ISR_FOVW|ISR_RXOVW|ISR_ROK))
    out16(RTLREG(ISR), ISR_FOVW|ISR_RXOVW|ISR_ROK);

  pic_sendeoi();
}


void rtl8139_open(struct netdev *dev UNUSED) {
  return;
}

void rtl8139_close(struct netdev *dev UNUSED) {
  return;
}

int rtl8139_tx(struct netdev *dev UNUSED, struct pktbuf *pkt) {
  mutex_lock(&rtldev.txqueue_mtx);
  int result = queue_enqueue(&pkt->link, &rtldev.txqueue);
  if(result == 0)
    workqueue_add(tx_wq, rtl8139_tx_all, NULL);
  mutex_unlock(&rtldev.txqueue_mtx);
  return result;
}

struct pktbuf *rtl8139_rx(struct netdev *dev UNUSED) {
  mutex_lock(&rtldev.rxqueue_mtx);
  if(queue_is_empty(&rtldev.rxqueue)) {
    mutex_unlock(&rtldev.rxqueue_mtx);
    return NULL;
  }
  struct pktbuf *pkt = list_entry(queue_dequeue(&rtldev.rxqueue), struct pktbuf, link);
  mutex_unlock(&rtldev.rxqueue_mtx);
  return pkt;
}


