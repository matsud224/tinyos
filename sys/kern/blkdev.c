#include <kern/blkdev.h>
#include <kern/kernlib.h>
#include <kern/file.h>
#include <kern/fs.h>
#include <kern/page.h>
#include <kern/thread.h>
#include <kern/lock.h>

struct chunk {
  struct chunk *next_chunk;
  void *addr;
  void *freelist;
  int nobjs;
  int nfree;
};

static struct blkdev_ops *blkdev_tbl[MAX_BLKDEV];
static u16 nblkdev;

static struct chunk *chunklist;
static struct list_head buf_list[MAX_BLKDEV];
static mutex buf_list_mtx;
static struct list_head avail_list;
static struct blkbuf blkbufs[NBLKBUF];

int blkdev_file_open(struct file *f, int mode);
int blkdev_file_read(struct file *f, void *buf, size_t count);
int blkdev_file_write(struct file *f, const void *buf, size_t count);
int blkdev_file_lseek(struct file *f, off_t offset, int whence);
int blkdev_file_close(struct file *f);
int blkdev_file_sync(struct file *f);

const struct file_ops blkdev_file_ops = {
  .open= blkdev_file_open,
  .read = blkdev_file_read,
  .write = blkdev_file_write,
  .lseek = blkdev_file_lseek,
  .close = blkdev_file_close,
  .sync = blkdev_file_sync,
};

void blkdev_init() {
  mutex_init(&buf_list_mtx);

  for(int i=0; i<MAX_BLKDEV; i++)
    blkdev_tbl[i] = NULL;

  nblkdev = BAD_MAJOR + 1;

  list_init(&avail_list);
  for(int i=0; i<MAX_BLKDEV; i++)
    list_init(&buf_list[i]);

  for(int i=0; i<NBLKBUF; i++) {
    blkbufs[i].ref = 0;
    blkbufs[i].flags = 0;
    list_pushback(&blkbufs[i].avail_link, &avail_list);
    list_pushback(&blkbufs[i].dev_link, &buf_list[BAD_MAJOR]);
  }
}

int blkdev_register(struct blkdev_ops *ops) {
  if(nblkdev >= MAX_BLKDEV)
    return -1;
  blkdev_tbl[nblkdev] = ops;
  return nblkdev++;
}

static void *blkbuf_alloc() {
  struct chunk *c = chunklist;
  while(c!=NULL && c->nfree) c = c->next_chunk;
  if(c == NULL) {
    c = malloc(sizeof(struct chunk));
    c->next_chunk = chunklist;
    chunklist = c;
    c->freelist = NULL;
    c->nobjs = c->nfree = PAGESIZE/BLOCKSIZE;
    u8 *obj = page_alloc();
    c->addr = obj;
    for(int i=0; i<c->nobjs; i++) {
      *(void **)obj = c->freelist;
      c->freelist = obj;
      obj += BLOCKSIZE;
    }
  }
  void *addr = c->freelist;
  c->freelist = *(void **)(c->freelist);
  return addr;
}

static void blkbuf_free(void *addr) {
  u32 chunk_addr = (u32)addr & ~(PAGESIZE-1);
  struct chunk *c = chunklist;
  while(c!=NULL && c->addr == (void *)chunk_addr) c = c->next_chunk;
  if(c == NULL)
    return;
  *(void **)addr = c->freelist;
  c->freelist = addr;
}

static struct blkbuf *blkbuf_get_available() {
  struct blkbuf *buf;
IRQ_DISABLE
  while(list_is_empty(&avail_list)) {
    thread_sleep(&avail_list);
  }
  buf = list_entry(list_pop(&avail_list), struct blkbuf, avail_link);
  list_remove(&buf->dev_link);
IRQ_RESTORE
  blkbuf_flush(buf);
  return buf;
}

static int blkdev_check_major(devno_t devno) {
  int major = DEV_MAJOR(devno);
  if(blkdev_tbl[major] == NULL)
    return -1;
  else
    return 0;
}

struct blkbuf *blkbuf_get(devno_t devno, blkno_t blkno) {
  if(blkdev_check_major(devno))
    return NULL;

  mutex_lock(&buf_list_mtx);
  struct list_head *p;
  list_foreach(p, &buf_list[DEV_MAJOR(devno)]) {
    struct blkbuf *blk = list_entry(p, struct blkbuf, dev_link);
    if(blk->blkno == blkno) {
      if(blk->ref == 0)
        list_remove(&blk->avail_link);
      blk->ref++;
      mutex_unlock(&buf_list_mtx);
      return blk;
    }
  }

  struct blkbuf *newblk = blkbuf_get_available();
  newblk->ref = 1;
  newblk->devno = devno;
  newblk->blkno = blkno;
  newblk->addr = blkbuf_alloc();
  newblk->flags = 0;
  newblk->state = BB_ABSENT;
  list_pushfront(&newblk->dev_link, &buf_list[DEV_MAJOR(devno)]);
  mutex_unlock(&buf_list_mtx);
  return newblk;
}

void blkbuf_release(struct blkbuf *buf) {
  mutex_lock(&buf_list_mtx);
  buf->ref--;
  if(buf->ref == 0) {
    list_pushback(&buf->avail_link, &avail_list);
  }
  mutex_unlock(&buf_list_mtx);
}

int blkbuf_remove(struct blkbuf *buf) {
  mutex_lock(&buf_list_mtx);
  if(buf->ref > 0) {
    mutex_unlock(&buf_list_mtx);
    return -1;
  }

  list_remove(&buf->dev_link);
  list_pushback(&buf->avail_link, &avail_list);
  mutex_unlock(&buf_list_mtx);
  return 0;
}

int blkdev_open(devno_t devno) {
  if(blkdev_tbl[DEV_MAJOR(devno)] == NULL)
    return -1;
  return blkdev_tbl[DEV_MAJOR(devno)]->open(DEV_MINOR(devno));
}

int blkdev_close(devno_t devno) {
  if(blkdev_tbl[DEV_MAJOR(devno)] == NULL)
    return -1;
  return blkdev_tbl[DEV_MAJOR(devno)]->close(DEV_MINOR(devno));
}

static int blkdev_readreq(struct blkbuf *buf) {
  buf->flags &= ~BB_ERROR;
  buf->state = BB_PENDING;
  return blkdev_tbl[DEV_MAJOR(buf->devno)]->readreq(buf);
}

static int blkdev_writereq(struct blkbuf *buf) {
  buf->flags &= ~BB_ERROR;
  buf->state = BB_PENDING;
  return blkdev_tbl[DEV_MAJOR(buf->devno)]->writereq(buf);
}

int blkdev_wait(struct blkbuf *buf) {
IRQ_DISABLE
  while(buf->state == BB_PENDING) {
    thread_sleep(buf);
  }
IRQ_RESTORE

  if(buf->flags & BB_ERROR)
    return -1;

  return 0;
}

int blkbuf_read_async(struct blkbuf *buf) {
  int result = 0;

  if(buf->state == BB_ABSENT)
    result = blkdev_readreq(buf);

  return result;
}

int blkbuf_read(struct blkbuf *buf) {
  int result;
  result = blkbuf_read_async(buf);
  if(result)
    return result;
  return blkdev_wait(buf);
}

int blkbuf_readahead(struct blkbuf *buf, blkno_t ablk) {
  int result;
  result = blkbuf_read_async(buf);
  if(result)
    return result;
  struct blkbuf *abuf = blkbuf_get(buf->devno, ablk);
  result = blkbuf_read_async(abuf);
  blkbuf_release(abuf);
  if(result)
    return result;
  return blkdev_wait(buf);
}

int blkbuf_write_async(struct blkbuf *buf) {
  int result = 0;

  buf->flags |= BB_DIRTY;
  result = blkdev_writereq(buf);

  return result;
}

int blkbuf_write(struct blkbuf *buf) {
  int result;
  result = blkbuf_write_async(buf);
  if(result)
    return result;
  return blkdev_wait(buf);
}

int blkbuf_flush(struct blkbuf *buf) {
  int result = 0;
  if(buf->flags & BB_DIRTY)
    result = blkbuf_write(buf);
  return result;
}

void blkbuf_markdirty(struct blkbuf *buf) {
  buf->flags |= BB_DIRTY;
}

void blkbuf_iodone(struct blkbuf *buf) {
  buf->state = BB_DONE;
  if(buf->flags & BB_DIRTY)
    buf->flags &= ~BB_DIRTY;
}

void blkbuf_readerror(struct blkbuf *buf) {
  buf->flags |= BB_ERROR;
  buf->state = BB_ABSENT;
}

void blkbuf_writeerror(struct blkbuf *buf) {
  buf->flags |= BB_ERROR;
  buf->flags |= BB_DIRTY;
}

int blkdev_sync(devno_t devno) {
  if(blkdev_check_major(devno))
    return -1;

  mutex_lock(&buf_list_mtx);
  struct list_head *p;
  list_foreach(p, &buf_list[DEV_MAJOR(devno)]) {
    struct blkbuf *buf = list_entry(p, struct blkbuf, dev_link);
    blkbuf_flush(buf);
  }
  mutex_unlock(&buf_list_mtx);
  return 0;
}

int blkdev_sync_all() {
  for(int i=0; i<MAX_BLKDEV; i++) {
    blkdev_sync(i);
  }
  return 0;
}

int blkdev_file_open(struct file *f, int mode UNUSED) {
  struct vnode *vno = (struct vnode *)f->data;
  if(blkdev_check_major(vno->devno))
    return -1;

  if(f->ref == 1)
    return blkdev_open(vno->devno);
  else
    return 0;
}

int blkdev_file_read(struct file *f, void *buf, size_t count) {
  struct vnode *vno = (struct vnode *)f->data;
  int result;
  int remain = count;
  struct blkbuf *blk;

  while(count > 0) {
    blk = blkbuf_get(vno->devno, align(f->offset, BLOCKSIZE));
    result = blkbuf_read(blk);
    if(result) {
      blkbuf_release(blk);
      return count - remain;
    }
    int count_in_blk = MIN(BLOCKSIZE - (f->offset & (BLOCKSIZE-1)), remain);
    memcpy(buf, blk->addr, count_in_blk);
    buf = (u8 *)buf + count_in_blk;
    remain -= count_in_blk;
    blkbuf_release(blk);
  }

  return count - remain;
}

int blkdev_file_write(struct file *f, const void *buf, size_t count) {
  struct vnode *vno = (struct vnode *)f->data;
  int remain = count;
  struct blkbuf *blk;

  while(count > 0) {
    blk = blkbuf_get(vno->devno, align(f->offset, BLOCKSIZE));
    blkbuf_markdirty(blk);
    blkbuf_release(blk);
    int count_in_blk = MIN(BLOCKSIZE - (f->offset & (BLOCKSIZE-1)), remain);
    memcpy(blk->addr, buf, count_in_blk);
    buf = (u8 *)buf + count_in_blk;
    remain -= count_in_blk;
  }

  return count - remain;
}

int blkdev_file_lseek(struct file *f, off_t offset, int whence) {
  switch(whence) {
  case SEEK_SET:
    f->offset = offset;
    break;
  case SEEK_CUR:
    f->offset += offset;
    break;
  default:
    return -1;
  }
  return 0;
}

int blkdev_file_close(struct file *f) {
  struct vnode *vno = (struct vnode *)f->data;
  if(blkdev_check_major(vno->devno))
    return -1;

  blkdev_file_sync(f);

/*
  mutex_lock(&buf_list_mtx);
  struct list_head *p;
  list_foreach(p, &buf_list[DEV_MAJOR(vno->devno)]) {
    struct blkbuf *buf = list_entry(p, struct blkbuf, dev_link);
    blkbuf_remove(buf);
  }
  mutex_unlock(&buf_list_mtx);
*/

  return blkdev_close(vno->devno);
}

int blkdev_file_sync(struct file *f) {
  struct vnode *vno = (struct vnode *)f->data;
  if(blkdev_check_major(vno->devno))
    return -1;
  blkdev_sync(vno->devno);
  return 0;
}
