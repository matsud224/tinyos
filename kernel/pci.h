#pragma once
#include "kernlib.h"

struct pci_dev {
  struct list_head link;
  u8 bus;
  u8 dev;
  u8 func;
  u16 vendorid;
  u16 deviceid;
  u8 revid;
  u8 classcode;
  u8 hdrtype; 
};

enum field {
  PCI_DEVICEID			= 0x2,
  PCI_VENDORID			= 0x0,
  PCI_STATUS				= 0x6,
  PCI_COMMAND				= 0x4,
  PCI_CLASS					= 0xb,
  PCI_SUBCLASS			= 0xa,
  PCI_PROGIF				= 0x9,
  PCI_REVID					= 0x8,
  PCI_BIST					= 0xf,
  PCI_HEADERTYPE		= 0xe,
  PCI_LATAENCYTIMER	= 0xd,
  PCI_CACHELINESIZE	= 0xc,
  PCI_BAR0					= 0x10,
  PCI_BAR1					= 0x14,
  PCI_BAR2					= 0x18,
  PCI_BAR3					= 0x1c,
  PCI_BAR4					= 0x20,
  PCI_BAR5					= 0x24,
  PCI_INTLINE				= 0x3c,
};

static const char *PCI_CLASS_STR[0x12] = {
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


u32 pci_config_read32(struct pci_dev *pcidev, u8 offset);
u16 pci_config_read16(struct pci_dev *pcidev, u8 offset);
u8 pci_config_read8(struct pci_dev *pcidev, u8 offset);
void pci_config_write32(struct pci_dev *pcidev, u8 offset, u32 data);
void pci_config_write16(struct pci_dev *pcidev, u8 offset, u16 data);
void pci_config_write8(struct pci_dev *pcidev, u8 offset, u8 data);
struct pci_dev *pci_search_device(u16 vendorid, u16 deviceid);
void pci_init(void);
 
