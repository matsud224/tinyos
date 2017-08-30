#include "idt.h"
#include <stdint.h>

struct gatedesc {
  uint16_t baselo;
  uint16_t selector;
  uint8_t reserved;
  uint8_t flags;
  uint16_t basehi;
};

struct idtr {
  uint16_t limit;
  struct gatedesc *base;
};

struct gatedesc idt[256];
struct idtr idtr;

