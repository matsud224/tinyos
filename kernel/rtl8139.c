#include "rtl8139.h"
#include "idt.h"
#include "pic.h"
#include "kernlib.h"
#include "pci.h"
#include "params.h"

#define RTL8139_VENDORID 0x10ec
#define RTL8139_DEVICEID 0x8139

#define RTL8139_IDR				0x00
#define RTL8139_MAR				0x08
#define RTL8139_TSD				0x10
#define RTL8139_TSAD			0x20
#define RTL8139_RBSTART		0x30
#define RTL8139_ERBCR			0x34
#define RTL8139_ERSR			0x36
#define RTL8139_CR				0x37
#define RTL8139_CAPR			0x38
#define RTL8139_CBR				0x3a
#define RTL8139_IMR				0x3c
#define RTL8139_ISR				0x3e
#define RTL8139_TCR				0x40
#define RTL8139_RCR				0x44
#define RTL8139_TCTR			0x48
#define RTL8139_MPC				0x4c
#define RTL8139_9346CR		0x50
#define RTL8139_CONFIG0		0x51
#define RTL8139_CONFIG1		0x52

#define CR_BUFE		0x1
#define CR_TE			0x4
#define CR_RE			0x8
#define CR_RST			0x10

#define RCR_AAP			0x1
#define RCR_APM			0x2
#define RCR_AM			0x4
#define RCR_AR			0x8
#define RCR_WRAP		0x80
#define RCR_ALL_ACCEPT (RCR_AAP|RCR_APM|RCR_AM|RCR_AR)

#define PKTHDR_ROK	0x1
#define PKTHDR_FAE	0x2
#define PKTHDR_CRC	0x4
#define PKTHDR_LONG	0x8
#define PKTHDR_RUNT	0x10
#define PKTHDR_ISE	0x20
#define PKTHDR_BAR	0x2000
#define PKTHDR_PAM	0x4000
#define PKTHDR_MAR	0x8000

#define ISR_ROK			0x1
#define ISR_RER			0x2
#define ISR_TOK			0x4
#define ISR_TER			0x8
#define ISR_RXOVW		0x10
#define ISR_PUN			0x20
#define ISR_FOVW		0x40
#define ISR_LENCHG	0x2000
#define ISR_TIMEOUT	0x4000
#define ISR_SERR		0x8000

#define TSD_OWN			0x2000
#define TSD_TUN			0x4000
#define TSD_TOK			0x8000
#define TSD_CDH			0x10000000
#define TSD_OWC			0x20000000
#define TSD_TABT		0x40000000
#define TSD_CRS			0x80000000


#define RXBUF_SIZE			8192
#define RXBUF_PAD				16
#define RXBUF_WRAP_PAD	2048
#define RXBUF_TOTAL			(RXBUF_SIZE+RXBUF_PAD+RXBUF_WRAP_PAD)

static struct {
  struct pci_dev *pci;
  uint16_t iobase;
  uint8_t irq;
  uint8_t macaddr[6];
  uint8_t rxbuf[RXBUF_TOTAL];
  uint16_t rxbuf_index;
  uint32_t rxbuf_size;
  uint8_t cur_txreg;
} rtldev;

static const uint16_t TX_TSD[4] = {
  RTL8139_TSD, RTL8139_TSD+4, RTL8139_TSD+8, RTL8139_TSD+12
};

static const uint16_t TX_TSAD[4] = {
  RTL8139_TSAD, RTL8139_TSAD+4, RTL8139_TSAD+8, RTL8139_TSAD+12
};


void rtl8139_init(struct pci_dev *thisdev) {
  rtldev.pci = thisdev;
  rtldev.iobase = pci_config_read32(thisdev, PCI_BAR0);
  rtldev.iobase &= 0xfffc;
  rtldev.irq = pci_config_read8(thisdev, PCI_INTLINE);
  rtldev.rxbuf_index = 0;
  rtldev.cur_txreg = 0;
  for(int i=0; i<6; i++)
    rtldev.macaddr[i] = in8(rtldev.iobase+RTL8139_IDR+i);
  printf("iobase=%x\nirq=%x\nmacaddr=%x:%x:%x:%x:%x:%x\n",
    rtldev.iobase, rtldev.irq, 
    rtldev.macaddr[0], 
    rtldev.macaddr[1], 
    rtldev.macaddr[2], 
    rtldev.macaddr[3], 
    rtldev.macaddr[4], 
    rtldev.macaddr[5] );
  //enable PCI bus mastering
  uint16_t pci_cmd = pci_config_read16(thisdev, PCI_COMMAND);
  pci_cmd |= 0x4;
  //pci_config_write16(thisdev, PCI_COMMAND, pci_cmd);
#define PCI_CONFIG_ADDR 0xcf8
#define PCI_CONFIG_DATA 0xcfc
  uint32_t addr = (thisdev->bus<<16) | (thisdev->dev<<11) | (thisdev->func<<8) | (PCI_COMMAND) | 0x80000000u;
  out32(PCI_CONFIG_ADDR, addr);
  out16(PCI_CONFIG_DATA, pci_cmd);

//printf("pcicmd=%x\n", pci_config_read16(thisdev, PCI_COMMAND));
  //power on
  out8(rtldev.iobase+RTL8139_CONFIG1, 0x0);
  //software reset
  out8(rtldev.iobase+RTL8139_CR, CR_RST);
  while((in8(rtldev.iobase+RTL8139_CR)&CR_RST) != 0);
  //set rx buffer address
  out32(rtldev.iobase+RTL8139_RBSTART, rtldev.rxbuf-KERNSPACE_ADDR);
  //set IMR
  out16(rtldev.iobase+RTL8139_IMR, 0x55);
  //receive configuration
  out32(rtldev.iobase+RTL8139_RCR, RCR_ALL_ACCEPT | RCR_WRAP);
  //enable rx&tx
  out8(rtldev.iobase+RTL8139_CR, CR_RE|CR_TE);
  //setup interrupt
  idt_register(rtldev.irq+0x20, IDT_INTGATE, rtl8139_inthandler); 
  pic_clearmask(rtldev.irq);
}

int rtl8139_probe() {
  struct pci_dev *thisdev = pci_search_device(RTL8139_VENDORID, RTL8139_DEVICEID);
  rtl8139_init(thisdev);
  return (thisdev != NULL);
}

#define ETHER_ADDR_LEN	6
void packet_analyze(uint8_t *buf) {
  struct ether_hdr{
    uint8_t ether_dhost[ETHER_ADDR_LEN];
    uint8_t ether_shost[ETHER_ADDR_LEN];
    uint16_t ether_type;
  } *ehdr = buf;
  printf("from : %x:%x:%x:%x:%x:%x\n",
          ehdr->ether_shost[0], 
          ehdr->ether_shost[1], 
          ehdr->ether_shost[2], 
          ehdr->ether_shost[3], 
          ehdr->ether_shost[4], 
          ehdr->ether_shost[5]);
  free(buf);
}

struct ether_hdr{
  uint8_t ether_dhost[ETHER_ADDR_LEN];
  uint8_t ether_shost[ETHER_ADDR_LEN];
  uint16_t ether_type;
  uint8_t payload[128];
} epkt = {
  .ether_dhost = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
  .ether_shost = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x00},
  .ether_type = 0x0000,
  .payload = "This is a test packet."
};

int rtl8139_tx() {
  void *buf = &epkt;
  uint32_t size = sizeof(epkt);
  cli();
  out32(rtldev.iobase+TX_TSAD[rtldev.cur_txreg], buf-KERNSPACE_ADDR);
  out32(rtldev.iobase+TX_TSD[rtldev.cur_txreg], sizeof(epkt));
  rtldev.cur_txreg = (rtldev.cur_txreg+1)%4;
  printf("sent size=%d\n", sizeof(epkt));
  sti();
}

int rtl8139_rx() {
  uint32_t offset = rtldev.rxbuf_index % RXBUF_SIZE;
  uint32_t pkt_hdr = *((uint32_t *)(rtldev.rxbuf+offset));
  uint16_t rx_status = pkt_hdr&0xffff;
  uint16_t rx_size = pkt_hdr>>16;
  //printf("%x hdr=%x\n", rtldev.rxbuf+offset, pkt_hdr);
  if(rx_status & PKTHDR_RUNT ||
     rx_status & PKTHDR_LONG ||
     rx_status & PKTHDR_CRC ||
     rx_status & PKTHDR_FAE ||
     (rx_status & PKTHDR_ROK) == 0) {
    puts("bad packet.");
    goto out;
  }

  //printf("packet size=%d\n", rx_size-4);
  uint8_t *pktbuf = malloc(rx_size-4);
  memcpy(pktbuf, rtldev.rxbuf+offset+4, rx_size-4);
  packet_analyze(pktbuf);
  
out:
  rtldev.rxbuf_index = (offset + rx_size + 4 + 3) & ~3;
  out16(rtldev.iobase+RTL8139_CAPR, rtldev.rxbuf_index - 16);
}

void rtl8139_isr() {
  uint16_t isr = in16(rtldev.iobase+RTL8139_ISR);
  printf("tsd[0]=%x\n", in32(rtldev.iobase+TX_TSD[0]));
  //if((in32(rtldev.iobase+TX_TSD[0])&(TSD_OWN|TSD_TOK)) == (TSD_OWN|TSD_TOK)) {
  if(isr & ISR_TOK) {
    out16(rtldev.iobase+RTL8139_ISR, ISR_TOK);
    puts("send ok");
  }
  if(isr & (ISR_FOVW|ISR_RXOVW|ISR_ROK))
    out16(rtldev.iobase+RTL8139_ISR, ISR_FOVW|ISR_RXOVW|ISR_ROK);
  while((in8(rtldev.iobase+RTL8139_CR) & CR_BUFE) == 0)
    rtl8139_rx();
  pic_sendeoi();
}
