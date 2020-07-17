#include <stdint.h>
#include <stddef.h>
#include <kern/malloc.h>
#include <kern/page.h>
#include <kern/params.h>
#include <kern/kernlib.h>

#define SIZE_BASE		8
#define MAX_BIN			(((PAGESIZE - sizeof(struct chunkhdr)) >> 1) / SIZE_BASE)
#define USE_BIN_THRESHOLD (SIZE_BASE * MAX_BIN)

struct chunkhdr {
  struct list_head link;
  size_t size;
  void *freelist;
  int nobjs;
  int nfree;
};

struct list_head bin[MAX_BIN + 1];

void malloc_init() {
  for(unsigned int i = 0; i <= MAX_BIN; i++)
    list_init(&bin[i]);
}

static struct chunkhdr *getnewchunk(size_t objsize) {
  if(objsize < 4 || objsize > PAGESIZE - sizeof(struct chunkhdr)) {
    return NULL;
  }

  struct chunkhdr *newchunk = (struct chunkhdr *)page_alloc(PAGESIZE, 0);
  if(newchunk == NULL)
   puts("malloc: page_alloc failed.");

  newchunk->size = objsize;
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

  if(size > USE_BIN_THRESHOLD) {
    int npages =  (size + sizeof(struct chunkhdr) + (PAGESIZE-1)) & ~(PAGESIZE-1);
    struct chunkhdr *ch = page_alloc(PAGESIZE * npages, 0);
    ch->size = size;
    m = (void *)(ch + 1);
  }else {
    m = getobj(size / SIZE_BASE);
    if(m == NULL)
     puts("warn: malloc failed.");
  }
IRQ_RESTORE
  return m;
}

void free(void *addr) {
IRQ_DISABLE
  struct chunkhdr *ch = (struct chunkhdr *)pagealign((u32)addr);
  if (ch->size > USE_BIN_THRESHOLD) {
    page_free(ch);
  } else {
    ch->nfree++;
    if (ch->nfree == ch->nobjs) {
      list_remove(&ch->link);
      page_free(ch);
    } else {
      *(void **)addr = ch->freelist;
      ch->freelist = addr;
    }
  }
IRQ_RESTORE
}

/*
void *realloc(void *ptr, size_t size) {
  if (ptr == NULL) {
    return malloc(size);
  } else if (size == 0) {
    free(ptr);
    return NULL;
  }

  void *new = malloc(size);

}
*/

