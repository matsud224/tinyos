#include "pci.h"
#include "kernasm.h"
#include "vga.h"
#include <stdint.h>
#include <stddef.h>

#define PCI_CONFIG_ADDR 0xcf8
#define PCI_CONFIG_DATA 0xcfc

#define PCI_DEVICEID			0x2
#define PCI_VENDORID			0x0
#define PCI_STATUS				0x6
#define PCI_COMMAND				0x4
#define PCI_CLASS					0xb
#define PCI_SUBCLASS			0xa
#define PCI_PROGIF				0x9
#define PCI_REVID					0x8
#define PCI_BIST					0xf
#define PCI_HEADERTYPE		0xe
#define PCI_LATAENCYTIMER	0xd
#define PCI_CACHELINESIZE	0xc
#define PCI_BAR0					0x10
#define PCI_BAR1					0x14
#define PCI_BAR2					0x18
#define PCI_BAR3					0x1c
#define PCI_BAR4					0x20
#define PCI_BAR5					0x24
#define PCI_INTLINE				0x3c

static const char *classstr[0x12] = {
  "Device was built prior definition of the class code field",
  "Mass Storage Controller",
  "Network Controller",
  "Display Controller",
  "Multimedia Controller",
  "Memory Controller",
  "Bridge Device",
  "Simple Communication Controllers",
  "Base System Peripherals",
  "Input Devices",
  "Docking Stations",
  "Processors",
  "Serial Bus Controllers",
  "Wireless Controllers",
  "Intelligent I/O Controllers",
  "Satellite Communication Controllers",
  "Encryption/Decryption Controllers",
  "Data Acquisition and Signal Processing Controllers",
};

uint32_t pci_config_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
  uint32_t addr = (bus<<16) | (dev<<11) | (func<<8) | (offset&0xfc) | 0x80000000u;
  out32(PCI_CONFIG_ADDR, addr);
  return in32(PCI_CONFIG_DATA);
}

uint16_t pci_config_read16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
  uint32_t result = pci_config_read32(bus, dev, func, offset);
  return (result >> ((offset&2)*8)) & 0xffff;
}

uint8_t pci_config_read8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
  uint32_t result = pci_config_read32(bus, dev, func, offset);
  return (result >> ((offset&3)*8)) & 0xff;
}

static void pci_printinfo_one(uint8_t bus, uint8_t dev, uint8_t func) {
  uint8_t class, subclass, progif;
  uint32_t bar4;
  class = pci_config_read8(bus, dev, func, PCI_CLASS);
  subclass = pci_config_read8(bus, dev, func, PCI_SUBCLASS);
  progif = pci_config_read8(bus, dev, func, PCI_PROGIF);
  bar4 = pci_config_read32(bus, dev, func, PCI_BAR4);
  printf("%x:%x.%x %x %x %x BAR4=0x%x  %s\n", bus, dev, func, class, subclass, progif, bar4, (class<0x12)?classstr[class]:"Unknown");
}

void pci_printinfo() {
  for(int bus = 0; bus < 256; bus++) {
    for(int dev = 0; dev < 32; dev++) {
      if(pci_config_read16(bus, dev, 0, PCI_VENDORID) != 0xffff) {
        pci_printinfo_one(bus, dev, 0);
        if(pci_config_read8(bus, dev, 0, PCI_HEADERTYPE) & 0x80) {
          for(uint8_t func = 1; func < 8; func++)
            if(pci_config_read16(bus, dev, func, PCI_VENDORID) != 0xffff)
              pci_printinfo_one(bus, dev, func);
        }
      }
    }
  }
}
