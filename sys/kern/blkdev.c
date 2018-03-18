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
  blkbuf_sync(buf);
  return buf;
}

struct blkbuf *blkbuf_get(devno_t devno, blkno_t blkno) {
  struct list_head *p;
  list_foreach(p, &buf_list[DEV_MAJOR(devno)]) {
    struct blkbuf *blk = list_entry(p, struct blkbuf, dev_link);
    if(blk->blkno == blkno) {
      if(blk->ref == 0)
        list_pushback(&blk->avail_link, &avail_list);
      blk->ref++;
      return blk;
    }
  }

  struct blkbuf *newblk = blkbuf_get_available();
  newblk->ref = 1;
  newblk->devno = devno;
  newblk->blkno = blkno;
  newblk->addr = blkbuf_alloc();
  newblk->flags = BB_ABSENT;
  list_pushfront(&newblk->dev_link, &buf_list[DEV_MAJOR(devno)]);
  return newblk;
} 

void blkbuf_release(struct blkbuf *buf) {
  if(buf->ref == 1)
    list_pushback(&buf->avail_link, &avail_list);
  else
    buf->ref--;
}

int blkbuf_remove(struct blkbuf *buf) {
  if(buf->ref > 0)
    return -1;

  list_remove(&buf->dev_link);
  list_pushback(&buf->avail_link, &avail_list);
  return 0;
}


void blkbuf_markdirty(struct blkbuf *buf) {
  buf->flags |= BB_DIRTY;
}


int blkdev_open(devno_t devno) {
  return blkdev_tbl[DEV_MAJOR(devno)]->open(DEV_MINOR(devno));
}

int blkdev_close(devno_t devno) {
  return blkdev_tbl[DEV_MAJOR(devno)]->close(DEV_MINOR(devno));
}

static int blkdev_readreq(struct blkbuf *buf) {
  buf->flags |= BB_PENDING;
  buf->flags &= ~(BB_ABSENT | BB_DIRTY | BB_ERROR);
  return blkdev_tbl[DEV_MAJOR(buf->devno)]->readreq(buf);
}

static int blkdev_writereq(struct blkbuf *buf) {
  buf->flags |= BB_PENDING;
  buf->flags &= ~(BB_ABSENT | BB_DIRTY | BB_ERROR);
  return blkdev_tbl[DEV_MAJOR(buf->devno)]->writereq(buf);
}

int blkdev_wait(struct blkbuf *buf) {
IRQ_DISABLE
  while(buf->flags & BB_PENDING) {
    thread_sleep(buf);
  }
IRQ_RESTORE

  if(buf->flags & BB_ERROR) {
    buf->flags |= BB_ABSENT;
    return -1;
  }

  return 0;
}

int blkbuf_sync(struct blkbuf *buf) {
  int result = 0;
  if(buf->flags & BB_PENDING)
    return 0;

  if(buf->flags & BB_DIRTY)
    result = blkdev_writereq(buf);
  else if(buf->flags & BB_ABSENT)
    result = blkdev_readreq(buf);

  if(result)
    return result;

  return blkdev_wait(buf);
}


static int blkdev_check_major(devno_t devno) {
  int major = DEV_MAJOR(devno);
  if(blkdev_tbl[major] == NULL)
    return -1;
  else
    return 0;
}

int blkdev_file_open(struct file *f, int mode UNUSED) {
  struct vnode *vno = (struct vnode *)f->data;
  if(blkdev_check_major(DEV_MAJOR(vno->devno)))
    return -1;

  return blkdev_open(vno->devno);
}

int blkdev_file_read(struct file *f, void *buf, size_t count) {
  struct vnode *vno = (struct vnode *)f->data;
  int result;
  int remain = count;
  struct blkbuf *blk;

  while(count > 0) {
    blk = blkbuf_get(vno->devno, align(f->offset, BLOCKSIZE));
    result = blkbuf_sync(blk);
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
  int result;
  int remain = count;
  struct blkbuf *blk;

  while(count > 0) {
    blk = blkbuf_get(vno->devno, align(f->offset, BLOCKSIZE));
    result = blkbuf_sync(blk);
    if(result) {
      blkbuf_release(blk);
      return count - remain;
    }
    int count_in_blk = MIN(BLOCKSIZE - (f->offset & (BLOCKSIZE-1)), remain);
    memcpy(blk->addr, buf, count_in_blk);
    buf = (u8 *)buf + count_in_blk;
    remain -= count_in_blk;
    blkbuf_release(blk);
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
  if(blkdev_check_major(DEV_MAJOR(vno->devno)))
    return -1;

  blkdev_file_sync(f);

  struct list_head *p;
  list_foreach(p, &buf_list[DEV_MAJOR(vno->devno)]) {
    struct blkbuf *buf = list_entry(p, struct blkbuf, dev_link);
    blkbuf_remove(buf);
  }
  return blkdev_close(vno->devno);
}

int blkdev_file_sync(struct file *f) {
  struct vnode *vno = (struct vnode *)f->data;
  if(blkdev_check_major(vno->devno))
    return -1;

  struct list_head *p;
  list_foreach(p, &buf_list[DEV_MAJOR(vno->devno)]) {
    struct blkbuf *buf = list_entry(p, struct blkbuf, dev_link);
    blkbuf_sync(buf);
  }
  return 0;
}
