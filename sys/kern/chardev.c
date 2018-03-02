#include <kern/page.h>
#include <kern/chardev.h>
#include <kern/kernlib.h>
#include <kern/thread.h>

struct chardev_ops *chardev_tbl[MAX_CHARDEV];
static u16 nchardev;

void chardev_init() {
  for(int i=0; i<MAX_CHARDEV; i++)
    chardev_tbl[i] = NULL;
  nchardev = 0;
}

int chardev_register(struct chardev_ops *ops) {
  if(nchardev >= MAX_CHARDEV)
    return -1;
  chardev_tbl[nchardev] = dev;
  dev->devno = nchardev;
  return nchardev++;
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

int chardev_open(devno_t devno) {
  struct chardev_ops *ops = chardev_tbl[DEV_MAJOR(devno)];
  if(ops == NULL)
    return -1;

  return ops->open(DEV_MINOR(devno));
}

int chardev_close(devno_t devno) {
  struct chardev_ops *ops = chardev_tbl[DEV_MAJOR(devno)];
  if(ops == NULL)
    return -1;

  return ops->close(DEV_MINOR(devno));
}

u32 chardev_read(devno_t devno, u8 *dest, u32 count) {
  struct chardev_ops *ops = chardev_tbl[DEV_MAJOR(devno)];
  if(ops == NULL)
    return -1;

  u32 remain = count;
IRQ_DISABLE
  while(remain > 0) {
    u32 n = ops->read(DEV_MINOR(devno), dest, remain);
    remain -= n;
    dest += n;
    if(remain > 0)
      thread_sleep(ops);
  }
IRQ_RESTORE
  return count;
}

u32 chardev_write(devno_t devno, u8 *src, u32 count) {
  struct chardev_ops *ops = chardev_tbl[DEV_MAJOR(devno)];
  if(ops == NULL)
    return -1;

  u32 remain = count;
IRQ_DISABLE
  while(remain > 0) {
    u32 n = ops->write(DEV_MINOR(devno), src, count);
    remain -= n;
    src += n;
    if(remain > 0)
      thread_sleep(ops);
  }
IRQ_RESTORE
  return count;
}

