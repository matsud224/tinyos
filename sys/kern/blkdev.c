#include <kern/page.h>
#include <kern/kernasm.h>
#include <kern/blkdev.h>
#include <kern/kernlib.h>
#include <kern/thread.h>

struct blkdev *blkdev_tbl[MAX_BLKDEV];
static u16 nblkdev;

struct chunk {
  struct chunk *next_chunk;
  void *addr;
  void *freelist;
  int nobjs;
  int nfree;
};

static struct chunk *chunklist;

void blkdev_init() {
  for(int i=0; i<MAX_BLKDEV; i++)
    blkdev_tbl[i] = NULL;
  nblkdev = 0;
}

void blkdev_add(struct blkdev *dev) {
  blkdev_tbl[nblkdev] = dev;
  dev->devno = nblkdev;
  nblkdev++;
}

static void *bufallocator_alloc() {
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

static void bufallocator_free(void *addr) {
  u32 chunk_addr = (u32)addr & ~(PAGESIZE-1);
  struct chunk *c = chunklist;
  while(c!=NULL && c->addr == (void *)chunk_addr) c = c->next_chunk;
  if(c == NULL)
    return;
  *(void **)addr = c->freelist;
  c->freelist = addr;
}

struct blkdev_buf *blkdev_getbuf(devno_t devno, blkno_t blockno) {
  struct blkdev *dev = blkdev_tbl[devno];
  if(dev == NULL)
    return NULL;

  for(struct blkdev_buf *p=dev->buf_list; p!=NULL; p=p->next) {
    if(p->blockno == blockno) {
      p->ref++;
      return p;
    }
  }

  struct blkdev_buf *newbuf = malloc(sizeof(struct blkdev_buf));
  newbuf->ref = 1;
  newbuf->dev = dev;
  newbuf->blockno = blockno;
  newbuf->addr = bufallocator_alloc();
  newbuf->flags = BDBUF_EMPTY;
  newbuf->next = dev->buf_list;
  dev->buf_list = newbuf;
  return newbuf;
} 

void blkdev_releasebuf(struct blkdev_buf *buf) {
  if(buf->ref != 0)
    buf->ref--;
}

static void waitbuf(struct blkdev_buf *buf) {
  cli();
  while((buf->flags & BDBUF_READY) == 0) {
    thread_sleep(buf);
  }
  sti();
}

void blkdev_buf_sync_nowait(struct blkdev_buf *buf) {
  struct blkdev *dev = buf->dev;
  dev->ops->sync(buf);
}

void blkdev_buf_sync(struct blkdev_buf *buf) {
  if(buf->flags & BDBUF_READY)
    return;
  blkdev_buf_sync_nowait(buf);
  waitbuf(buf);
}

void blkdev_buf_markdirty(struct blkdev_buf *buf) {
  buf->flags |= BDBUF_DIRTY;
}


