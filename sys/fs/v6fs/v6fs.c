#include <kern/kernlib.h>
#include <kern/fs.h>
#include <kern/blkdev.h>

#define V6FS_BOOT 0
#define V6FS_SUPERBLK 1
#define V6FS_INODECACHE_SIZE 512

static struct fs *v6fs_mount(void *source);
static struct inode *v6fs_getroot(struct fs *fs);
static int v6fs_inode_read(struct inode *inode, u8 *base, u32 offset, u32 count);
static struct inode *v6fs_inode_opdent(struct inode *inode, const char *name, int op);
static struct inode *v6fs_getinode(struct fs *fs, u32 ino_no);


static const struct fsinfo_ops v6fs_fsinfo_ops = {
  .mount = v6fs_mount
};

static const struct fsinfo v6fs_info = {
  .name = "v6fs",
  .ops = &v6fs_fsinfo_ops
};

static const struct fs_ops v6fs_fs_ops = {
  .getroot = v6fs_getroot
};

struct v6fs_super {
  u16 ino_blocks;
  u16 sto_blocks;
  u16 sto_nfree;
  u16 sto_free[100];
  u16 ino_nfree;
  u16 ino_free[100];
  volatile u8 sto_lock;
  volatile u8 ino_lock;
  u8 is_mod;
  u8 is_ro;
  u16 time[2];
  u16 pad[50];
};

struct v6fs_fs {
  u16 devno;
  struct v6fs_super super;
  struct fs fs;
};

#define I_ALLOC 0x8000
#define I_DIR 0x4000
#define I_LARGE 0x1000

struct v6fs_phyinode {
  u16 mode;
  u8 nlink;
  u8 uid;
  u8 gid;
  u8 size0;
  u16 size1;
  u16 blkno[8];
  u16 atime[2];
  u16 mtime[2];
};

static const struct inode_ops v6fs_inode_ops = {
  .read = v6fs_inode_read,
  .write = NULL,
  .resize = NULL,
  .opdent = v6fs_inode_opdent
};

struct v6fs_inode {
  u16 blkno[8];
  struct inode inode;
};

struct v6fs_dent {
  u16 inode_no;
  char name[14];
};


FS_INIT void v6fs_init() {
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
  u16 devno = (u16)source;
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

static struct inode *v6fs_getinode(struct fs *fs, u32 ino_no) {
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
static u32 blkno_vtop(struct v6fs_inode *v6ino, u32 vno) {
  struct v6fs_fs *f = container_of(v6ino->inode.fs, struct v6fs_fs, fs);
  u16 devno = f->devno;
  if((v6ino->inode.mode & I_LARGE) == 0) {
    return v6ino->blkno[vno];
  } else {
    int indir_index = vno/256;
    if(indir_index <= 6) {
      struct blkdev_buf *buf = blkdev_getbuf(devno, v6ino->blkno[indir_index]);
      blkdev_buf_sync(buf);
      u32 pno = ((u16 *)(buf->addr))[vno%256];
      blkdev_releasebuf(buf);
      return pno;
    } else {
      struct blkdev_buf *buf = blkdev_getbuf(devno, v6ino->blkno[7]);
      blkdev_buf_sync(buf);
      u32 indir2_no = ((u16 *)(buf->addr))[(indir_index-7)/256];
      blkdev_releasebuf(buf); 
      struct blkdev_buf *buf2 = blkdev_getbuf(devno, indir2_no);
      blkdev_buf_sync(buf2);
      u32 pno = ((u16 *)(buf2->addr))[(indir_index-7)%256];
      blkdev_releasebuf(buf2); 
      return pno;
    }
  }
  return 0;
}

static int v6fs_inode_read(struct inode *inode, u8 *base, u32 offset, u32 count) {
  u32 tail = count + offset;
  tail = (tail > inode->size) ? inode->size : tail;

  struct v6fs_fs *f = container_of(inode->fs, struct v6fs_fs, fs);
  u16 devno = f->devno;
  struct v6fs_inode *v6ino = container_of(inode, struct v6fs_inode, inode);
  struct blkdev_buf *buf = NULL;

  for(u32 i=offset; i<tail; i++) {
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
  u16 found = 0;
  if((inode->mode & I_ALLOC) == 0)
    goto exit;
  if((inode->mode & I_DIR) == 0)
    goto exit;
  for(u32 i=0; i<inode->size; i+=16) {
    if(i%BLOCKSIZE == 0) {
      if(buf != NULL)
        blkdev_releasebuf(buf);
      buf = blkdev_getbuf(devno, blkno_vtop(v6ino, i/BLOCKSIZE));
      blkdev_buf_sync(buf);
    }

    struct v6fs_dent *dent = (struct v6fs_dent*)((u8 *)(buf->addr)+(i%512));
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
