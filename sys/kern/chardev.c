#include <kern/page.h>
#include <kern/chardev.h>
#include <kern/kernlib.h>
#include <kern/thread.h>

const struct chardev_ops *chardev_tbl[MAX_CHARDEV];
static u16 nchardev;

int chardev_file_read(struct file *f, void *buf, size_t count);
int chardev_file_write(struct file *f, const void *buf, size_t count);
int chardev_file_close(struct file *f);
int chardev_file_sync(struct file *f);

const struct file_ops chardev_file_ops = {
  .read = chardev_file_read,
  .write = chardev_file_write,
  .close = chardev_file_close,
  .sync = chardev_file_sync,
};

void chardev_init() {
  for(int i=0; i<MAX_CHARDEV; i++)
    chardev_tbl[i] = NULL;
  nchardev = BAD_MAJOR + 1;
}

int chardev_register(const struct chardev_ops *ops) {
  if(nchardev >= MAX_CHARDEV)
    return -1;
  chardev_tbl[nchardev] = ops;
  return nchardev++;
}

struct chardev_buf *cdbuf_create(char *mem, size_t size) {
  struct chardev_buf *buf = malloc(sizeof(struct chardev_buf));
  buf->size = size;
  buf->free = size;
  buf->head = 0;
  buf->tail = 0;
  buf->addr = mem;
  return buf;
}

size_t cdbuf_read(struct chardev_buf *buf, char *dest, size_t count) {
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

size_t cdbuf_write(struct chardev_buf *buf, const char *src, size_t count) {
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
  const struct chardev_ops *ops = chardev_tbl[DEV_MAJOR(devno)];
  if(ops == NULL)
    return -1;

  return ops->open(DEV_MINOR(devno));
}

int chardev_close(devno_t devno) {
  const struct chardev_ops *ops = chardev_tbl[DEV_MAJOR(devno)];
  if(ops == NULL)
    return -1;

  return ops->close(DEV_MINOR(devno));
}

size_t chardev_read(devno_t devno, char *dest, size_t count) {
  const struct chardev_ops *ops = chardev_tbl[DEV_MAJOR(devno)];
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

size_t chardev_write(devno_t devno, const char *src, size_t count) {
  const struct chardev_ops *ops = chardev_tbl[DEV_MAJOR(devno)];
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


static int chardev_check_major(devno_t devno) {
  int major = DEV_MAJOR(devno);
  if(chardev_tbl[major] == NULL)
    return -1;
  else
    return 0;
}

int chardev_file_open(struct file *f) {
  struct vnode *vno = (struct vnode *)f->data;
  if(chardev_check_major(DEV_MAJOR(vno->devno)))
    return -1;

  return chardev_close(vno->devno);
}

int chardev_file_read(struct file *f, void *buf, size_t count) {
  struct vnode *vno = (struct vnode *)f->data;
  if(chardev_check_major(DEV_MAJOR(vno->devno)))
    return -1;

  return chardev_read(vno->devno, buf, count);
}

int chardev_file_write(struct file *f, const void *buf, size_t count) {
  struct vnode *vno = (struct vnode *)f->data;
  if(chardev_check_major(DEV_MAJOR(vno->devno)))
    return -1;

  return chardev_write(vno->devno, buf, count);
}

int chardev_file_close(struct file *f) {
  struct vnode *vno = (struct vnode *)f->data;
  if(chardev_check_major(DEV_MAJOR(vno->devno)))
    return -1;

  return chardev_close(vno->devno);
}

int chardev_file_sync(struct file *f UNUSED) {
  return -1;
}

