#include <kern/page.h>
#include <kern/chardev.h>
#include <kern/kernlib.h>
#include <kern/thread.h>

const struct chardev_ops *chardev_tbl[MAX_CHARDEV];
static u16 nchardev;

int chardev_file_open(struct file *f);
int chardev_file_read(struct file *f, void *buf, size_t count);
int chardev_file_write(struct file *f, const void *buf, size_t count);
int chardev_file_close(struct file *f);
int chardev_file_sync(struct file *f);

const struct file_ops chardev_file_ops = {
  .open = chardev_file_open,
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

int cdbuf_read(struct chardev_buf *buf, char *dest, size_t count) {
  int read_count = 0;
  while(read_count < count && buf->free < buf->size) {
    *dest++ = buf->addr[buf->tail++];
    if(buf->tail == buf->size)
      buf->tail = 0;
    read_count++;
    buf->free++;
  }
  return read_count;
}

int cdbuf_write(struct chardev_buf *buf, const char *src, size_t count) {
  int write_count = 0;
  while(write_count < count && buf->free > 0) {
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

static int fill_linebuf(devno_t devno, struct chardev_state *state) {
  const struct chardev_ops *ops = chardev_tbl[DEV_MAJOR(devno)];

  if(state->linebuf_head > 0 && state->linebuf[state->linebuf_head-1] == '\n')
    return 0;

  int result = 0;
  while(1) {
    int n = ops->read(DEV_MINOR(devno), state->tempbuf, CDTEMPBUFSIZE);
    if(n < 0) {
      result = -1;
      goto exit;
    }
    for(int i = 0; i < n; i++) {
      switch(state->tempbuf[i]) {
      case 0x7f:
        if(state->linebuf_head > 0)
          state->linebuf_head--;
        break;
      case '\r':
      case '\n':
        if(state->linebuf_head == MAX_CHARDEV_LINE_CHARS)
          state->linebuf_head--;
        state->linebuf[state->linebuf_head++] = '\n';
        goto exit;
      default:
        if(state->linebuf_head < MAX_CHARDEV_LINE_CHARS)
          state->linebuf[state->linebuf_head++] = state->tempbuf[i];
        break;
      }
    }

    thread_sleep(ops);
  }

exit:
  return result;
}

int chardev_read(devno_t devno, char *dest, size_t count) {
  const struct chardev_ops *ops = chardev_tbl[DEV_MAJOR(devno)];
  if(ops == NULL)
    return -1;

  struct chardev_state *state = ops->getstate(DEV_MINOR(devno));

  int read_bytes = 0;

  if(state->mode & CDMODE_CANON) {
IRQ_DISABLE
    if(fill_linebuf(devno, state) < 0) {
      read_bytes = -1;
      break;
    }
    read_bytes = MIN(count, state->linebuf_head);
    memcpy(dest, state->linebuf, read_bytes);
    int rem_bytes = state->linebuf_head - read_bytes;
    for(int i=0; i < rem_bytes; i++)
      state->linebuf[i] = state->linebuf[read_bytes+i];
    state->linebuf_head = rem_bytes;
IRQ_RESTORE
  } else {
    int remain = count;
IRQ_DISABLE
    while(remain > 0) {
      int n = ops->read(DEV_MINOR(devno), dest, remain);
      if(n < 0) {
        read_bytes = -1;
        break;
      }
      remain -= n;
      dest += n;
      read_bytes += n;
      if(remain > 0)
        thread_sleep(ops);
    }
IRQ_RESTORE
  }

  return read_bytes;
}


int chardev_write(devno_t devno, const char *src, size_t count) {
  const struct chardev_ops *ops = chardev_tbl[DEV_MAJOR(devno)];
  if(ops == NULL)
    return -1;

  int remain = count;
IRQ_DISABLE
  while(remain > 0) {
    int n = ops->write(DEV_MINOR(devno), src, remain);
    if(n < 0) {
      count = -1;
      break;
    }
    remain -= n;
    src += n;
    if(remain > 0)
      thread_sleep(ops);
  }
IRQ_RESTORE
  return count;
}

void chardev_initstate(struct chardev_state *state, int mode) {
  state->mode = mode;
  state->linebuf_head = 0;
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
  if(chardev_check_major(vno->devno))
    return -1;

  return chardev_open(vno->devno);
}

int chardev_file_read(struct file *f, void *buf, size_t count) {
  struct vnode *vno = (struct vnode *)f->data;
  if(chardev_check_major(vno->devno))
    return -1;

  return chardev_read(vno->devno, buf, count);
}

int chardev_file_write(struct file *f, const void *buf, size_t count) {
  struct vnode *vno = (struct vnode *)f->data;
  if(chardev_check_major(vno->devno))
    return -1;

  return chardev_write(vno->devno, buf, count);
}

int chardev_file_close(struct file *f) {
  struct vnode *vno = (struct vnode *)f->data;
  if(chardev_check_major(vno->devno))
    return -1;

  return chardev_close(vno->devno);
}

int chardev_file_sync(struct file *f UNUSED) {
  return -1;
}

