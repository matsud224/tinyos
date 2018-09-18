#include <stdint.h>
#include <stddef.h>
#include <kern/malloc.h>
#include <kern/page.h>
#include <kern/params.h>
#include <kern/kernlib.h>

#define SIZE_BASE		8
#define MAX_BIN			(((PAGESIZE - sizeof(struct chunkhdr)) >> 1) / SIZE_BASE)

struct chunkhdr {
  struct list_head link;
  void *freelist;
  int nobjs;
  int nfree;
};

struct list_head bin[MAX_BIN];

void malloc_init() {
  for(unsigned int i=0; i<MAX_BIN; i++)
    list_init(&bin[i]);
}

static struct chunkhdr *getnewchunk(size_t objsize) {
  if(objsize < 4 || objsize > PAGESIZE-sizeof(struct chunkhdr)) {
    return NULL;
  }

  struct chunkhdr *newchunk = (struct chunkhdr *)page_alloc();
  if(newchunk == NULL)
   puts("malloc: page_alloc failed.");

  newchunk->freelist = NULL;
  int nobjs = (PAGESIZE - sizeof(struct chunkhdr)) / objsize;
  newchunk->nobjs = newchunk->nfree = nobjs;
  u8 *obj = (u8 *)(newchunk+1);
  for(int i=0; i<nobjs; i++) {
    *(void **)obj = newchunk->freelist;
    newchunk->freelist = obj;
    obj += objsize;
  }

  return newchunk;
}

static void *getobj(int binindex) {
  struct list_head *p;
retry:
  list_foreach(p, &bin[binindex]) {
    struct chunkhdr *ch = list_entry(p, struct chunkhdr, link);
    if(ch->nfree > 0) {
      void *obj = ch->freelist;
      ch->freelist = *(void **)obj;
      ch->nfree--;
      return obj;
    }
  }

  struct chunkhdr *newch = getnewchunk(binindex * SIZE_BASE);
  if(newch == NULL)
    return NULL;

  list_pushback(&newch->link, &bin[binindex]);

  goto retry;
}

void *malloc(size_t request) {
  void *m = NULL;
IRQ_DISABLE
  size_t size = (request + (SIZE_BASE-1)) & ~(SIZE_BASE-1);
  if(size > SIZE_BASE * MAX_BIN && size <= PAGESIZE) {
    m = page_alloc();
  }else if(size > SIZE_BASE * MAX_BIN) {
    printf("malloc: objsize=%d byte is not supported.", size);
  }else {
    m = getobj(size/SIZE_BASE);
    if(m == NULL)
     puts("warn: malloc failed.");
  }
IRQ_RESTORE
  return m;
}

void free(void *addr) {
IRQ_DISABLE
  /*if(((vaddr_t)addr & (PAGESIZE-1)) == 0) {
    page_free(addr);
  } else */{
    struct chunkhdr *ch = (struct chunkhdr *)((u32)addr&~(PAGESIZE-1));
    ch->nfree++;
    if(ch->nfree == ch->nobjs) {
      list_remove(&ch->link);
      page_free(ch);
    } else {
      *(void **)addr = ch->freelist;
      ch->freelist = addr;
    }
  }
IRQ_RESTORE
}
