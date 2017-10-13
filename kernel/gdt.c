#include "gdt.h"
#include "task.h"
#include "kernasm.h"
#include "kernlib.h"

#define DESC_SEGMENT 0x10
#define DESC_DATASEG 0x0
#define DESC_CODESEG 0x8
#define DESC_WRITABLE 0x2
#define DESC_READABLE 0x2

#define DESC_TSS 0x0
#define DESC_TSS32 0x9

#define DESC_DPL_0 0x0
#define DESC_DPL_1 0x20
#define DESC_DPL_2 0x40
#define DESC_DPL_3 0x60

#define DESC_PRESENT 0x80

#define DESC_DB 0x40
#define DESC_G 0x80

#define GDT_NULL			0
#define GDT_CODESEG_0	1
#define GDT_DATASEG_0	2
#define GDT_CODESEG_3	3
#define GDT_DATASEG_3	4
#define GDT_TSS				5

static struct descriptor {
  u16 limit;
  u16 baselo;
  u8 basemid;
  u8 flag0;
  u8 flag1;
  u8 basehi;
} PACKED gdt[6] = {
  //null
  {0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00},
  //code segment(ring 0)
  {0xffff, 0x0000, 0x00, DESC_SEGMENT|DESC_CODESEG|DESC_READABLE|DESC_DPL_0|DESC_PRESENT, DESC_DB|DESC_G|0xf, 0x00},
  //data segment(ring 0)
  {0xffff, 0x0000, 0x00, DESC_SEGMENT|DESC_DATASEG|DESC_WRITABLE|DESC_DPL_0|DESC_PRESENT, DESC_DB|DESC_G|0xf, 0x00},
  //code segment(ring 3)
  {0xffff, 0x0000, 0x00, DESC_SEGMENT|DESC_CODESEG|DESC_READABLE|DESC_DPL_3|DESC_PRESENT, DESC_DB|DESC_G|0xf, 0x00},
  //data segment(ring 3)
  {0xffff, 0x0000, 0x00, DESC_SEGMENT|DESC_DATASEG|DESC_WRITABLE|DESC_DPL_3|DESC_PRESENT, DESC_DB|DESC_G|0xf, 0x00},
  //tss
  {sizeof(struct tss)&0xffff, 0x0000, 0x00, DESC_TSS|DESC_TSS32|DESC_DPL_0, sizeof(struct tss)>>16, 0x00},
};

static struct gdtr {
  u16 limit;
  struct descriptor *base;
} PACKED gdtr; 

void gdt_init() {
  gdtr.limit = sizeof(gdt)-1;
  gdtr.base = gdt;
  lgdt(&gdtr);
}

void gdt_settssbase(void *base) {
  gdt[GDT_TSS].baselo = (u32)base & 0xffff;
  gdt[GDT_TSS].basemid = ((u32)base>>16) & 0xff;
  gdt[GDT_TSS].basehi = (u32)base >> 24;
  gdt[GDT_TSS].flag0 |= DESC_PRESENT;
}


