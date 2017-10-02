#include "page.h"
#include "malloc.h"
#include "chardev.h"
#include <stdint.h>
#include <stddef.h>

struct chardev *chardev_tbl[MAX_CHARDEV];
static uint16_t nchardev;

void chardev_init() {
  for(int i=0; i<MAX_BLKDEV; i++)
    chardev_tbl[i] = NULL;
  nchardev = 0;
}

void chardev_add(struct chardev *dev) {
  chardev_tbl[nchardev] = dev;
  dev->devno = nchardev;
  nchardev++;
}

struct chardev_buf *chardevbuf_create(uint8_t *mem, uint32_t size) {
  struct chardev_buf *buf = malloc(sizeof(struct chardev_buf));
  buf->size = size;
  buf->free = size;
  buf->head = 0;
  buf->tail = 0;
  buf->addr = mem;
  return buf;
}

uint32_t chardevbuf_read(struct chardev_buf *buf, uint8_t *dest, uint32_t count) {
  uint32_t read_count = 0;
  while((read_count < count) && (buf->head != buf->tail)) {
    *dest++ = buf->addr[buf->tail++];
    if(buf->tail == buf->size)
      buf->tail = 0;
    read_count++;
    buf->free++;
  }
  return read_count;
}

uint32_t chardevbuf_write(struct chardev_buf *buf, uint8_t *src, uint32_t count) {
  uint32_t write_count = 0;
  uint32_t limit = (buf->tail+(buf->size-1))%buf->size;
  while((write_count < count) && (buf->head != limit)) {
    buf->addr[buf->head++] = *src++;
    if(buf->head == buf->size)
      buf->head = 0;
    write_count++;
    buf->free--;
  }
  return write_count;
}

uint32_t chardev_read(uint16_t devno, uint8_t *dest, uint32_t count) {
  struct chardev *dev = chardev_tbl[devno];
  uint32_t remain = count;
  while(remain > 0) {
    cli();
    uint32_t n = dev->ops->read(dev, dest, remain);
    remain -= n;
    dest += n;
    if(remain > 0)
      task_sleep(dev);
    sti();
  }
  return count;
}

uint32_t chardev_write(uint16_t devno, uint8_t *src, uint32_t count) {
  struct chardev *dev = chardev_tbl[devno];
  uint32_t remain = count;
  while(remain > 0) {
    cli();
    uint32_t n = dev->ops->write(dev, src, count);
    remain -= n;
    src += n;
    if(remain > 0)
      task_sleep(dev);
    sti();
  }
  return count;
}

