#include "idt.h"
#include "params.h"
#include "kernasm.h"

#define IDT_PRESENT 0x80
#define IDT_GATE32 0x8

#define IDTSIZE 256

static struct gatedesc {
  uint16_t baselo;
  uint16_t selector;
  uint8_t reserved;
  uint8_t flags;
  uint16_t basehi;
} PACKED idt[IDTSIZE];

static struct idtr {
  uint16_t limit;
  struct gatedesc *base;
} PACKED idtr; 


void idt_register(uint8_t vecnum, uint8_t gatetype, void (*base)(void)) {
  struct gatedesc *desc = &idt[vecnum];
  desc->selector = GDT_SEL_CODESEG_0;
  desc->baselo = (uint32_t)base & 0xffff;
  desc->basehi = (uint32_t)base >> 16;
  desc->reserved = 0;
  desc->flags = gatetype | IDT_PRESENT | IDT_GATE32;
}

void idt_unregister(uint8_t vecnum) {
  idt[vecnum].flags = 0;
}

void idt_init() {
  for(int i=0; i<IDTSIZE; i++)
    idt_unregister(i);
  
  idtr.limit = IDTSIZE * sizeof(struct gatedesc);
  idtr.base = idt;
  lidt(&idtr);
}

