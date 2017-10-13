#include <stdint.h>
#include <stddef.h>
#include "malloc.h"
#include "page.h"
#include "common.h"
#include "params.h"
#include "vga.h"

#define SIZE_BASE 8 
#define MAX_BIN 512 

// chunk = 1page
// chunk consists of objects
struct chunkhdr {
  void *next_chunk;
  void *freelist;
  int nobjs;
  int nfree;
};

struct chunkhdr *bin[MAX_BIN];

void malloc_init() {
  for(int i=0; i<MAX_BIN; i++)
    bin[i] = NULL;
}

static struct chunkhdr *getnewchunk(size_t objsize) {
  if(objsize < 4 || objsize > PAGESIZE-sizeof(struct chunkhdr))
    return NULL;

  struct chunkhdr *newchunk = (struct chunkhdr *)page_alloc();
  newchunk->next_chunk = NULL;
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
  struct chunkhdr *ch;
retry:
  ch = bin[binindex];
  while(ch != NULL) {
    if(ch->nfree > 0) {
      void *obj = ch->freelist;
      ch->freelist = *(void **)obj;
      ch->nfree--;
      return obj;
    }
    ch = ch->next_chunk;
  }

  struct chunkhdr *newch = getnewchunk(binindex * SIZE_BASE);
  if(newch == NULL)
    return NULL;
  newch->next_chunk = bin[binindex];
  bin[binindex] = newch;

  goto retry;
}

void *malloc(size_t request) {
  size_t size = (request + (SIZE_BASE-1)) & ~(SIZE_BASE-1);
  if(size > SIZE_BASE * MAX_BIN)
    return NULL;

  return getobj(size/SIZE_BASE);
}

void free(void *addr) {
  struct chunkhdr *ch = (struct chunkhdr *)((u32)addr&~(PAGESIZE-1));
  *(void **)addr = ch->freelist;
  ch->freelist = addr;
  ch->nfree++;
}
