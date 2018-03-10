#include <kern/kernlib.h>
#include <kern/fs.h>
#include <kern/blkdev.h>

#define MINIX3_BOOT 0
#define MINIX3_SUPERBLK 1
#define MINIX3_INODECACHE_SIZE 512

static struct fs *minix3_mount(void *source);
static struct inode *minix3_getroot(struct fs *fs);
static int minix3_inode_read(struct inode *inode, u8 *base, u32 offset, u32 count);
static struct inode *minix3_inode_opdent(struct inode *inode, const char *name, int op);
static struct inode *minix3_getinode(struct fs *fs, u32 ino_no);

int minix3_read(struct file *f, void *buf, size_t count);
int minix3_write(struct file *f, const void *buf, size_t count);
int minix3_lseek(struct file *f, off_t offset, int whence);
int minix3_close(struct file *f);
int minix3_sync(struct file *f);

static const struct file_ops minix3_file_ops = {
  .read = minix3_read,
  .write = minix3_write,
  .lseek = minix3_lseek, 
  .close = minix3_close,
  .sync = minix3_sync,
};

struct vnode *minix3_create(struct vnode *vno, const char *name);
struct vnode *minix3_lookup(struct vnode *vno, const char *name);
int minix3_mknod(struct vnode *vno, const char *name);
int minix3_link(struct vnode *vno, const char *name);
int minix3_unlink(struct vnode *vno, const char *name);
int minix3_stat(struct vnode *vno, struct stat *buf);

static const struct vnode_ops minix3_vnode_ops = {
	.create = minix3_create,
	.lookup = minix3_lookup,
	.mknod = minix3_mknod, 
	.link = minix3_link,
	.unlink = minix3_unlink,
	.stat = minix3_stat,
};


static const struct fsinfo_ops minix3_fsinfo_ops = {
  .mount = minix3_mount
};

static const struct fsinfo minix3_info = {
  .name = "minix3",
  .ops = &minix3_fsinfo_ops
};

static const struct fs_ops minix3_fs_ops = {
  .getroot = minix3_getroot
};

struct minix3_inode {
  u16 i_mode;
  u16 i_nlinks;
  u16 i_uid;
  u16 i_gid;
  u32 i_size;
  u32 i_atime;
  u32 i_mtime;
  u32 i_ctime;
  u32 i_zone[10];
} PACKED;

struct minix3_super_block {
  u32 s_ninodes;
  u16 s_pad0;
  u16 s_imap_blocks;
  u16 s_zmap_blocks;
  u16 s_firstdatazone;
  u16 s_log_zone_size;
  u16 s_pad1;
  u32 s_max_size;
  u32 s_zones;
  u16 s_magic;
  u16 s_pad2;
  u16 s_blocksize;
  u8  s_disk_version;
} PACKED;

struct minix3_dir_entry {
  u32 inode;
  char name[30];
};

struct minix3_fs {
  u16 devno;
  struct minix3_super super;
  struct fs fs;
};

#define MINIX_BLOCK_SIZE_BITS 10
#define MINIX_BLOCK_SIZE     (1 << MINIX_BLOCK_SIZE_BITS)

#define MINIX_NAME_MAX       255             /* # chars in a file name */
#define MINIX_MAX_INODES     65535

#define MINIX2_INODES_PER_BLOCK ((MINIX_BLOCK_SIZE)/(sizeof (struct minix2_inode)))

#define MINIX_VALID_FS       0x0001          /* Clean fs. */
#define MINIX_ERROR_FS       0x0002          /* fs has errors. */

#define MINIX3_SUPER_MAGIC   0x4d5a          /* minix V3 fs (60 char names) */

#define 

struct minix3_inode {
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

static const struct inode_ops minix3_vnode_ops = {
};

struct minix3_vnode {
  u16 blkno[8];
  struct vnode inode;
};


FS_INIT void minix3_init() {
  fsinfo_add(&minix3_info);
}

static int minix3_is_valid_super(struct minix3_super *super) {
  if(super->ino_blocks == 0 || super->sto_blocks == 0)
    return 0;
  if(super->ino_nfree > 100 || super->sto_nfree > 100)
    return 0;
  return 1;
}

static struct fs *minix3_mount(void *source) {
  u16 devno = (u16)source;
  struct minix3_fs *minix3 = malloc(sizeof(struct minix3_fs));
  minix3->devno = devno;
  struct blkdev_buf *buf = blkdev_getbuf(devno, MINIX3_SUPERBLK);
  blkdev_buf_sync(buf);
  minix3->super = *(struct minix3_super *)(buf->addr);
  blkdev_releasebuf(buf);
  minix3->fs.ops = &minix3_fs_ops;
  if(!minix3_is_valid_super(&(minix3->super))) {
    free(minix3);
    return NULL;
  }    
  return &(minix3->fs);
}

static struct inode *minix3_getroot(struct fs *fs) {
  return minix3_getinode(fs, 1);
}

static struct inode *minix3_getinode(struct fs *fs, u32 ino_no) {
  //inode番号は1から
  struct minix3_fs *minix3 = container_of(fs, struct minix3_fs, fs);
  int inoblk = (ino_no+31) / 16;
  int inooff = (ino_no+31) % 16;
  struct blkdev_buf *buf = blkdev_getbuf(minix3->devno, inoblk);
  blkdev_buf_sync(buf);
  struct minix3_phyinode *pinode = (struct minix3_phyinode *)(buf->addr);
  pinode += inooff;
  struct minix3_inode *v6ino = malloc(sizeof(struct minix3_inode));
  for(int i=0; i<8; i++) {
    v6ino->blkno[i] = pinode->blkno[i];
  }
  v6ino->inode.fs = fs;
  v6ino->inode.ops = &minix3_inode_ops;
  v6ino->inode.inode_no = ino_no;
  v6ino->inode.mode = pinode->mode;
  v6ino->inode.size = (pinode->size0<<16) | pinode->size1;
  blkdev_releasebuf(buf);
  return &(v6ino->inode);
}

// 論理ブロック番号を物理ブロック番号へ変換
static u32 blkno_vtop(struct minix3_inode *v6ino, u32 vno) {
  struct minix3_fs *f = container_of(v6ino->inode.fs, struct minix3_fs, fs);
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

static int minix3_inode_read(struct inode *inode, u8 *base, u32 offset, u32 count) {
  u32 tail = count + offset;
  tail = (tail > inode->size) ? inode->size : tail;

  struct minix3_fs *f = container_of(inode->fs, struct minix3_fs, fs);
  u16 devno = f->devno;
  struct minix3_inode *v6ino = container_of(inode, struct minix3_inode, inode);
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

static struct inode *minix3_dentop(struct inode *inode, const char *name, int op) {
  struct minix3_fs *f = container_of(inode->fs, struct minix3_fs, fs);
  devno_t devno = f->devno;
  struct minix3_inode *v6ino = container_of(inode, struct minix3_inode, inode);
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

    struct minix3_dent *dent = (struct minix3_dent*)((u8 *)(buf->addr)+(i%512));
    if(dent->inode_no == 0)
      continue;
    switch(op) {
    case OP_LOOKUP:
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
  case OP_LOOKUP:
    if(found == 0)
      return NULL;
    else
      return minix3_getinode(inode->fs, found);
  break;
  }

  return NULL;
}


int minix3_read(struct socket *s, void *buf, size_t count) {
  return recv(s, buf, count, 0);
}

int minix3_write(struct socket *s, const void *buf, size_t count) {
  return send(s, buf, count, 0);
}

int minix3_lseek(struct socket *s, off_t offset, int whence) {
  return EBADF;
}

int minix3_close(struct socket *s) {
  int retval = s->ops->close(s->pcb);

  mutex_lock(&socklist_mtx);
  list_remove(&s->link);
  mutex_unlock(&socklist_mtx);

  return retval;
}

int minix3_sync(struct socket *s) {
  return 0;
}

struct vnode *minix3_create(struct vnode *vno, const char *name) {
  return NULL;
}

int minix3_mknod(struct vnode *vno, int mode, dev_t devno) {
  return -1;
}

int minix3_link(struct vnode *vno, const char *name) {
  return -1;
}

int minix3_unlink(struct vnode *vno, const char *name) {
  return -1;
}

int minix3_stat(struct vnode *vno, struct stat *buf) {
  struct minix3_vno *fatvno = container_of(vno, struct minix3_vnode, vnode);
  struct minix3_fs *f = container_of(vno->fs, struct minix3_fs, fs);

  bzero(buf, sizeof(struct stat));
  buf->st_dev = f->devno;
  buf->st_size = fatvno->size;

  return 0;
}

