#include "rtl8139.h"
#include "pci.h"

#define RTL8139_VENDORID 0x10ec
#define RTL8139_DEVICEID 0x8139

#define RTL8139_IDR				0x00
#define RTL8139_MAR				0x08
#define RTL8139_RBSTART		0x30
#define RTL8139_CMD				0x37
#define RTL8139_IMR				0x3c
#define RTL8139_ISR				0x3e

int rtl8139_probe() {
  struct pci_dev *thisdev = pci_search_device(RTL8139_VENDORID, RTL8139_DEVICEID);
  if(thisdev == NULL)
    puts("RTL8139 not found.");
  else
    puts("RTL8139 found!");
  uint32_t iobase = pci_config_read32(thisdev, PCI_BAR0);
  iobase &= 0xfffffffc;
  printf("mac addr=%x:%x:%x:%x:%x:%x\n", in8(iobase), in8(iobase+1), in8(iobase+2), in8(iobase+3), in8(iobase+4), in8(iobase+5));
  return 0;
}


