#include "v6fs.h"
#include "v6fs.h"
#include "fs.h"
#include "malloc.h"
#include "blkdev.h"
#include "common.h"
#include "vga.h"
#include <stdint.h>
#include <stddef.h>

#define V6FS_BOOT 0
#define V6FS_SUPERBLK 1
#define V6FS_INODECACHE_SIZE 512

static struct fs *v6fs_mount(void *source);
static struct inode *v6fs_getroot(struct fs *fs);
static int v6fs_inode_read(struct inode *inode, uint8_t *base, uint32_t offset, uint32_t count);
static struct inode *v6fs_inode_opdent(struct inode *inode, const char *name, int op);
static struct inode *v6fs_getinode(struct fs *fs, uint32_t ino_no);


struct fsinfo_ops v6fs_fsinfo_ops = {
  .mount = v6fs_mount
};

struct fsinfo v6fs_info = {
  .name = "v6fs",
  .ops = &v6fs_fsinfo_ops
};

struct fs_ops v6fs_fs_ops = {
  .getroot = v6fs_getroot
};

struct v6fs_super {
  uint16_t ino_blocks;
  uint16_t sto_blocks;
  uint16_t sto_nfree;
  uint16_t sto_free[100];
  uint16_t ino_nfree;
  uint16_t ino_free[100];
  volatile uint8_t sto_lock;
  volatile uint8_t ino_lock;
  uint8_t is_mod;
  uint8_t is_ro;
  uint16_t time[2];
  uint16_t pad[50];
};

struct v6fs_fs {
  uint16_t devno;
  struct v6fs_super super;
  struct fs fs;
};

#define I_ALLOC 0x8000
#define I_DIR 0x4000
#define I_LARGE 0x1000

struct v6fs_phyinode {
  uint16_t mode;
  uint8_t nlink;
  uint8_t uid;
  uint8_t gid;
  uint8_t size0;
  uint16_t size1;
  uint16_t blkno[8];
  uint16_t atime[2];
  uint16_t mtime[2];
};

struct inode_ops v6fs_inode_ops = {
  .read = v6fs_inode_read,
  .write = NULL,
  .resize = NULL,
  .opdent = v6fs_inode_opdent
};

struct v6fs_inode {
  uint16_t blkno[8];
  struct inode inode;
};

struct v6fs_dent {
  uint16_t inode_no;
  char name[14];
};


void v6fs_init() {
  fsinfo_add(&v6fs_info);
}

static int v6fs_is_valid_super(struct v6fs_super *super) {
  if(super->ino_blocks == 0 || super->sto_blocks == 0)
    return 0;
  if(super->ino_nfree > 100 || super->sto_nfree > 100)
    return 0;
  return 1;
}

static struct fs *v6fs_mount(void *source) {
  uint16_t devno = (uint16_t)source;
  struct v6fs_fs *v6fs = malloc(sizeof(struct v6fs_fs));
  v6fs->devno = devno;
  struct blkdev_buf *buf = blkdev_getbuf(devno, V6FS_SUPERBLK);
  blkdev_buf_sync(buf);
  v6fs->super = *(struct v6fs_super *)(buf->addr);
  blkdev_releasebuf(buf);
  v6fs->fs.ops = &v6fs_fs_ops;
  if(!v6fs_is_valid_super(&(v6fs->super))) {
    free(v6fs);
    return NULL;
  }    
  return &(v6fs->fs);
}

static struct inode *v6fs_getroot(struct fs *fs) {
  return v6fs_getinode(fs, 1);
}

static struct inode *v6fs_getinode(struct fs *fs, uint32_t ino_no) {
  //inode番号は1から
  struct v6fs_fs *v6fs = container_of(fs, struct v6fs_fs, fs);
  int inoblk = (ino_no+31) / 16;
  int inooff = (ino_no+31) % 16;
  struct blkdev_buf *buf = blkdev_getbuf(v6fs->devno, inoblk);
  blkdev_buf_sync(buf);
  struct v6fs_phyinode *pinode = (struct v6fs_phyinode *)(buf->addr);
  pinode += inooff;
  struct v6fs_inode *v6ino = malloc(sizeof(struct v6fs_inode));
  for(int i=0; i<8; i++) {
    v6ino->blkno[i] = pinode->blkno[i];
  }
  v6ino->inode.fs = fs;
  v6ino->inode.ops = &v6fs_inode_ops;
  v6ino->inode.inode_no = ino_no;
  v6ino->inode.mode = pinode->mode;
  v6ino->inode.size = (pinode->size0<<16) | pinode->size1;
  blkdev_releasebuf(buf);
  return &(v6ino->inode);
}

// 論理ブロック番号を物理ブロック番号へ変換
static uint32_t blkno_vtop(struct v6fs_inode *v6ino, uint32_t vno) {
  struct v6fs_fs *f = container_of(v6ino->inode.fs, struct v6fs_fs, fs);
  uint16_t devno = f->devno;
  if((v6ino->inode.mode & I_LARGE) == 0) {
    return v6ino->blkno[vno];
  } else {
    int indir_index = vno/256;
    if(indir_index <= 6) {
      struct blkdev_buf *buf = blkdev_getbuf(devno, v6ino->blkno[indir_index]);
      blkdev_buf_sync(buf);
      uint32_t pno = ((uint16_t *)(buf->addr))[vno%256];
      blkdev_releasebuf(buf);
      return pno;
    } else {
      struct blkdev_buf *buf = blkdev_getbuf(devno, v6ino->blkno[7]);
      blkdev_buf_sync(buf);
      uint32_t indir2_no = ((uint16_t *)(buf->addr))[(indir_index-7)/256];
      blkdev_releasebuf(buf); 
      struct blkdev_buf *buf2 = blkdev_getbuf(devno, indir2_no);
      blkdev_buf_sync(buf2);
      uint32_t pno = ((uint16_t *)(buf2->addr))[(indir_index-7)%256];
      blkdev_releasebuf(buf2); 
      return pno;
    }
  }
  return 0;
}

static int v6fs_inode_read(struct inode *inode, uint8_t *base, uint32_t offset, uint32_t count) {
  uint32_t tail = count + offset;
  tail = (tail > inode->size) ? inode->size : tail;

  struct v6fs_fs *f = container_of(inode->fs, struct v6fs_fs, fs);
  uint16_t devno = f->devno;
  struct v6fs_inode *v6ino = container_of(inode, struct v6fs_inode, inode);
  struct blkdev_buf *buf = NULL;

  for(uint32_t i=offset; i<tail; i++) {
    if(buf == NULL || i%BLOCKSIZE==0) {
      if(buf != NULL)
        blkdev_releasebuf(buf);
      buf = blkdev_getbuf(devno, blkno_vtop(v6ino, i/BLOCKSIZE));
      blkdev_buf_sync(buf);
    }
    *base++ = buf->addr[i%BLOCKSIZE];
  }

  if(buf != NULL)
    blkdev_releasebuf(buf);

  return tail-offset;
}

static int strcmp_dent(const char *path, const char *name) {
  while(*path && *path!='/' && *path == *name) {
    path++; name++;
  }
  if(*path == '/' && *name == '\0')
    return 0;
  return *path - *name;
}

static struct inode *v6fs_inode_opdent(struct inode *inode, const char *name, int op) {
  struct v6fs_fs *f = container_of(inode->fs, struct v6fs_fs, fs);
  devno_t devno = f->devno;
  struct v6fs_inode *v6ino = container_of(inode, struct v6fs_inode, inode);
  struct blkdev_buf *buf = NULL;
  uint16_t found = 0;
  if((inode->mode & I_ALLOC) == 0)
    goto exit;
  if((inode->mode & I_DIR) == 0)
    goto exit;
  for(uint32_t i=0; i<inode->size; i+=16) {
    if(i%BLOCKSIZE == 0) {
      if(buf != NULL)
        blkdev_releasebuf(buf);
      buf = blkdev_getbuf(devno, blkno_vtop(v6ino, i/BLOCKSIZE));
      blkdev_buf_sync(buf);
    }

    struct v6fs_dent *dent = (struct v6fs_dent*)((uint8_t *)(buf->addr)+(i%512));
    if(dent->inode_no == 0)
      continue;
    switch(op) {
    case DENTOP_GET:
      if(strcmp_dent(name, dent->name) == 0) {
        found = dent->inode_no;
        goto exit;
      }
      break;
    default:
      // not implemented
      break;
    }
  }

exit:
  if(buf != NULL)
    blkdev_releasebuf(buf);

  switch(op) {
  case DENTOP_GET:
    if(found == 0)
      return NULL;
    else
      return v6fs_getinode(inode->fs, found);
  break;
  }

  return NULL;
}
