#include <kern/blkdev.h>
#include <kern/kernlib.h>

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
static struct list_head avail_list;
static struct blkbuf blkbufs[NBLKBUF];

DEV_INIT void blkdev_init() {
  for(int i=0; i<MAX_BLKDEV; i++)
    blkdev_tbl[i] = NULL;

  nblkdev = 0;

  list_init(&avail_list);
  for(int i=0; i<MAX_BLKDEV; i++)
    list_init(&buf_list[i]);

  for(int i=0; i<NBLKBUF; i++) {
    blkbufs[i].ref = 0;
    blkbufs[i].flags = 0;
    list_pushback(&blkbufs[i].avail_link, avail_list);
  }
}

int blkdev_register(struct blkdev_ops *ops) {
  if(nblkdev >= MAX_BLKDEV)
    return -1;
  blkdev_tbl[nblkdev] = ops;
  dev->devno = nblkdev;
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

static struct blkbuf *blkbuf_get_avail() {
  struct blkbuf *buf;
IRQ_DISABLE
  while(list_is_empty(&avail_list)) {
    thread_wait(&avail_list);
  }
  buf = list_pop(&avail_list);
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

  struct blkbuf *newblk = blkbuf_get_avail();
  newblk->ref = 1;
  newblk->devno = devno;
  newblk->blkno = blkno;
  newblk->addr = blkbuf_alloc();
  newblk->flags = BB_ABSENT;
  list_pushfront(&newblk->dev_link, &dev->buf_list);
  return newblk;
} 

void blkbuf_release(struct blkbuf *buf) {
  if(buf->ref == 1)
    list_pushback(&buf->avail_link, &avail_list);
  else
    buf->ref--;
}

void blkbuf_markdirty(struct blkbuf *buf) {
  buf->flags |= BB_DIRTY;
}


int blkdev_open(devno_t devno) {
  return blkdev_tbl[DEV_MAJOR(devno)]->ops->readblk(DEV_MINOR[devno], buf, blkno);
}

int blkdev_close(devno_t devno) {
  return blkdev_tbl[DEV_MAJOR(devno)]->ops->readblk(DEV_MINOR[devno], buf, blkno);
}

static int blkdev_readreq(struct blkbuf *buf) {
  buf->flags |= BB_PENDING;
  buf->flags &= ~(BB_ABSENT | BB_DIRTY | BB_ERROR);
  return blkdev_tbl[DEV_MAJOR(buf->devno)]->ops->readreq(buf);
}

static int blkdev_writereq(struct blkbuf *buf) {
  buf->flags |= BB_PENDING;
  buf->flags &= ~(BB_ABSENT | BB_DIRTY | BB_ERROR);
  return blkdev_tbl[DEV_MAJOR(buf->devno)]->ops->writereq(buf);
}

int blkdev_wait(struct blkbuf *buf) {
IRQ_DISABLE
  while(buf->flags & BB_PENDING) {
    thread_sleep(status);
  }
IRQ_RESTORE

  if(buf->flags & BB_ERROR) {
    buf->flags |= BB_ABSENT;
    return -1;
  }

  return 0;
}

void blkbuf_sync(struct blkbuf *buf) {
  if(buf->flags & BB_PENDING)
    return;

  if(buf->flags & BB_DIRTY)
    blkdev_writereq(buf);
  else if(buf->flags & BB_ABSENT)
    blkdev_readreq(buf);

  blkdev_wait(buf);
}


