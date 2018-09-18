#include <kern/vga.h>
#include <kern/lock.h>
#include <kern/params.h>
#include <kern/kernlib.h>
#include <kern/chardev.h>

int use_chardev = 0;

static struct {
  size_t column;
  size_t row;
  u8 color;
  u16 *buffer;
} vga;

#define VGAENTRY_COLOR(fg, bg) ((fg) | (bg) << 4)
#define VGAENTRY(uc, color) ((u16)(uc) | (u16)(color) << 8)

void vga_init() {
	vga.row = 0;
	vga.column = 0;
	vga.color = VGAENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
	vga.buffer = (u16*)PHYS_TO_KERN_VMEM(VRAM_COLOR);
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			vga.buffer[y*VGA_WIDTH+x] = VGAENTRY(' ', vga.color);
		}
	}
}

void vga_setcolor(u8 fg, u8 bg) {
	vga.color = VGAENTRY_COLOR(fg, bg);
}

static void vga_putentryat(char c, u8 color, size_t x, size_t y) {
	vga.buffer[y*VGA_WIDTH+x] = VGAENTRY(c, color);
}

static void scroll(size_t lines) {
  for(size_t y=0; y < VGA_HEIGHT-lines; y++)
     for(size_t x=0; x<VGA_WIDTH; x++)
       vga.buffer[y*VGA_WIDTH+x] = vga.buffer[(y+lines)*VGA_WIDTH+x];
  for(size_t y=VGA_HEIGHT-lines; y<VGA_HEIGHT; y++)
     for(size_t x=0; x<VGA_WIDTH; x++)
       vga.buffer[y*VGA_WIDTH+x] = VGAENTRY(' ', vga.color);
}

void switch_to_chardev() {
  use_chardev = 1;
}

int putchar(int c) {
  if(use_chardev) {
    if(c == 0x7f) {
      chardev_write(DEVNO(1, 0), "\b \b", 3);
    } else if(c == '\r') {
      chardev_write(DEVNO(1, 0), "\n", 1);
    } else {
      chardev_write(DEVNO(1, 0), &c, 1);
    }
    return c;
  }

  switch(c) {
  case '\n':
    vga.column = 0;
    vga.row++;
    break;
  case '\t':
    vga.column = (vga.column+VGA_TABWIDTH)/VGA_TABWIDTH*VGA_TABWIDTH;
    break;
  default:
	  vga_putentryat(c, vga.color, vga.column, vga.row);
    vga.column++;
    break;
	}

	if (vga.column >= VGA_WIDTH) {
	  vga.column = 0;
    vga.row++;
  }
  if (vga.row >= VGA_HEIGHT) {
    scroll(vga.row-VGA_HEIGHT+1);
    vga.row = VGA_HEIGHT-1;
  }

  return c;
}

static int putstr(const char *str) {
  while(*str) putchar(*str++);
  return 0;
}

int puts(const char *str) {
IRQ_DISABLE
  putstr(str);
  putchar('\n');
IRQ_RESTORE
  return 0;
}


static char buf[16];
static const char numchar[] =
  {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
   'a', 'b', 'c', 'd', 'e', 'f' };

static void print_num_signed(int32_t val, int base) {
  char *ptr = buf;
  *ptr++ = '\0';
  if(val < 0) {
    putchar('-');
    val = -val;
  }
  do {
    *ptr++ = numchar[val%base];
    val /= base;
  } while(val);
  while(*(--ptr))
    putchar(*ptr);
}

static void print_num_unsigned(u32 val, int base) {
  char *ptr = buf;
  *ptr++ = '\0';
  do {
    *ptr++ = numchar[val%base];
    val /= base;
  } while(val);
  while(*(--ptr))
    putchar(*ptr);
}


void printf(const char *fmt, ...) {
IRQ_DISABLE
  int *argbase = ((int *)&fmt)+1;
  while(*fmt) {
    switch(*fmt) {
    case '%':
      switch(*(++fmt)) {
      case 'd':
        print_num_signed(*argbase++, 10);
        break;
      case 'u':
        print_num_unsigned(*argbase++, 10);
        break;
      case 'x':
        print_num_unsigned(*argbase++, 16);
        break;
      case 'c':
        putchar((char)*argbase++);
        break;
      case 's':
        putstr((char *)*argbase++);
        break;
      case '%':
        putchar('%');
        break;
      }
      break;
    default:
      putchar(*fmt);
      break;
    }
    fmt++;
  }
IRQ_RESTORE
  return;
}



