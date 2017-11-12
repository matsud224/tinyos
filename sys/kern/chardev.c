#include <kern/page.h>
#include <kern/chardev.h>
#include <kern/kernlib.h>
#include <kern/task.h>

struct chardev *chardev_tbl[MAX_CHARDEV];
static u16 nchardev;

void chardev_init() {
  for(int i=0; i<MAX_CHARDEV; i++)
    chardev_tbl[i] = NULL;
  nchardev = 0;
}

void chardev_add(struct chardev *dev) {
  chardev_tbl[nchardev] = dev;
  dev->devno = nchardev;
  nchardev++;
}

struct chardev_buf *cdbuf_create(u8 *mem, u32 size) {
  struct chardev_buf *buf = malloc(sizeof(struct chardev_buf));
  buf->size = size;
  buf->free = size;
  buf->head = 0;
  buf->tail = 0;
  buf->addr = mem;
  return buf;
}

u32 cdbuf_read(struct chardev_buf *buf, u8 *dest, u32 count) {
  u32 read_count = 0;
  while((read_count < count) && (buf->head != buf->tail)) {
    *dest++ = buf->addr[buf->tail++];
    if(buf->tail == buf->size)
      buf->tail = 0;
    read_count++;
    buf->free++;
  }
  return read_count;
}

u32 cdbuf_write(struct chardev_buf *buf, u8 *src, u32 count) {
  u32 write_count = 0;
  u32 limit = (buf->tail+(buf->size-1))%buf->size;
  while((write_count < count) && (buf->head != limit)) {
    buf->addr[buf->head++] = *src++;
    if(buf->head == buf->size)
      buf->head = 0;
    write_count++;
    buf->free--;
  }
  return write_count;
}

u32 chardev_read(u16 devno, u8 *dest, u32 count) {
  struct chardev *dev = chardev_tbl[devno];
  u32 remain = count;
  while(remain > 0) {
    cli();
    u32 n = dev->ops->read(dev, dest, remain);
    remain -= n;
    dest += n;
    if(remain > 0)
      task_sleep(dev);
    sti();
  }
  return count;
}

u32 chardev_write(u16 devno, u8 *src, u32 count) {
  struct chardev *dev = chardev_tbl[devno];
  u32 remain = count;
  while(remain > 0) {
    cli();
    u32 n = dev->ops->write(dev, src, count);
    remain -= n;
    src += n;
    if(remain > 0)
      task_sleep(dev);
    sti();
  }
  return count;
}

