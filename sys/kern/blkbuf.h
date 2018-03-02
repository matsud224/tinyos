#pragma once
#include <kern/kernlib.h>
#include <kern/lock.h>
#include <kern/blkdev.h>

struct blkbuf *blkbuf_get_noalloc(devno_t devno, blkno_t blkno, void *buf);
struct blkbuf *blkbuf_getbuf(devno_t devno, blkno_t blockno);
void blkdev_releasebuf(struct blkinfo *buf);
void blkinfo_sync_nowait(struct blkinfo *buf);
void blkinfo_sync(struct blkinfo *buf);
void blkinfo_markdirty(struct blkinfo *buf);

