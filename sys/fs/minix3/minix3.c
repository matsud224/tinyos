#include <kern/kernlib.h>
#include <kern/fs.h>
#include <kern/file.h>
#include <kern/blkdev.h>

#define MINIX3_BOOTBLOCK	mblk_to_blk(0)
#define MINIX3_SUPERBLOCK	mblk_to_blk(1)

#define MINIX3_INVALID_INODE	0
#define MINIX3_ROOT_INODE			1

#define MINIX3_INVALID_ZONE		0

#define MINIX_BLOCK_SIZE_BITS	10
#define MBLOCKSIZE						(1 << MINIX_BLOCK_SIZE_BITS)

#define MINIX3_MAX_NAME_LEN		60
#define MINIX3_MAX_INODES			65535
#define MINIX3_MAX_LINK				65530

#define MINIX3_INDIRECT_DEPTH				3
#define MINIX3_INDIRECT_ZONE				7
#define MINIX3_DOUBLE_INDIRECT_ZONE	8
#define MINIX3_TRIPLE_INDIRECT_ZONE	9

#define MINIX3_SUPER_MAGIC		0x4d5a         /* minix V3 fs (60 char names) */

#define BITS_PER_MBLOCK	(MBLOCKSIZE << 3)
#define BITS_PER_BLOCK	(BITS_PER_MBLOCK / BLOCKS_PER_MBLOCK)

#define UPPER(size,n)	((size+((n)-1))/(n))

#define BLOCKS_PER_MBLOCK	(MBLOCKSIZE / BLOCKSIZE)

#define mblk_to_blk(b)			((b) * BLOCKS_PER_MBLOCK)
#define zone_to_blk(sb, z)	(mblk_to_blk((z) << (sb)->s_log_zone_size))

#define get_inodemapblk(sb)		mblk_to_blk(2)
#define get_zonemapblk(sb)		mblk_to_blk(2 + (sb)->s_imap_blocks)
#define get_inodetableblk(sb)	mblk_to_blk(2 + (sb)->s_imap_blocks + (sb)->s_zmap_blocks)
#define get_datazoneblk(sb)		mblk_to_blk((sb)->s_firstdatazone)


int minix3_read(struct file *f, void *buf, size_t count);
int minix3_write(struct file *f, const void *buf, size_t count);
int minix3_lseek(struct file *f, off_t offset, int whence);
int minix3_close(struct file *f);
int minix3_sync(struct file *f);
int minix3_truncate(struct file *f, size_t size);
int minix3_getdents(struct file *f, struct dirent *dirp, size_t count);

static const struct file_ops minix3_file_ops = {
  .read = minix3_read,
  .write = minix3_write,
  .lseek = minix3_lseek, 
  .close = minix3_close,
  .sync = minix3_sync,
  .truncate = minix3_truncate,
  .getdents = minix3_getdents, 
};

int minix3_lookup(struct vnode *vno, const char *name, struct vnode **found);
int minix3_mknod(struct vnode *parent, const char *name, int mode, devno_t devno);
int minix3_link(struct vnode *parent, const char *name, struct vnode *vno);
int minix3_unlink(struct vnode *parent, const char *name, struct vnode *vno);
int minix3_stat(struct vnode *vno, struct stat *buf);
void minix3_vfree(struct vnode *vno);
void minix3_vsync(struct vnode *vno);

static const struct vnode_ops minix3_vnode_ops = {
	.lookup = minix3_lookup,
	.mknod = minix3_mknod, 
	.link = minix3_link,
	.unlink = minix3_unlink,
	.stat = minix3_stat,
	.vfree = minix3_vfree,
	.vsync = minix3_vsync,
};

enum minix3_dent_ops {
  OP_LOOKUP,
  OP_EMPTY_CHECK,
  OP_ADD,
  OP_REMOVE,
};

static struct fs *minix3_mount(devno_t devno);

static const struct fstype_ops minix3_fstype_ops = {
  .mount = minix3_mount,
};

static struct vnode *minix3_getroot(struct fs *fs);

static const struct fs_ops minix3_fs_ops = {
  .getroot = minix3_getroot,
};

typedef u32 ino_t;
typedef u32 zone_t;

struct minix3_inode {
  u16 i_mode;
  u16 i_nlinks;
  u16 i_uid;
  u16 i_gid;
  u32 i_size;
  u32 i_atime;
  u32 i_mtime;
  u32 i_ctime;
  zone_t i_zone[10];
} PACKED;

struct minix3_sb {
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

struct minix3_dent {
  ino_t inode;
  char name[MINIX3_MAX_NAME_LEN];
} PACKED;

struct minix3_fs {
  devno_t devno;
  struct minix3_sb sb;
  struct fs fs;
  u32 zone_size;
  u32 blocks_in_zone;
  u32 zones_in_indirect_zone;
  zone_t zone_boundary[MINIX3_INDIRECT_DEPTH+1];
  u32 zone_divisor[MINIX3_INDIRECT_DEPTH];
  blkno_t imap_search_pos;
  blkno_t zmap_search_pos;
  mutex imap_mtx;
  mutex zmap_mtx;
  mutex vnode_mtx;
};

struct minix3_vnode {
  struct minix3_inode minix3;
  struct vnode vnode;
};

#define MINIX3_INODES_PER_MBLOCK	(MBLOCKSIZE / sizeof (struct minix3_inode))
#define MINIX3_DENTS_PER_MBLOCK		(MBLOCKSIZE / sizeof(struct minix3_dent))
#define MINIX3_INODES_PER_BLOCK		(MINIX3_INODES_PER_MBLOCK / BLOCKS_PER_MBLOCK)
#define MINIX3_DENTS_PER_BLOCK		(MINIX3_DENTS_PER_MBLOCK / BLOCKS_PER_MBLOCK)
#define INODE3_SIZE								(sizeof(struct minix3_inode))
#define MINIX3_DENT_SIZE								(sizeof(struct minix3_dent))

FS_INIT void minix3_init() {
  fstype_register("minix3", &minix3_fstype_ops);
}

static struct minix3_vnode *minix3_vnode_new(struct fs *fs, u32 number, struct minix3_inode *inode) {
  struct minix3_vnode *vno = malloc(sizeof(struct minix3_vnode));
  memcpy(&vno->minix3, inode, sizeof(struct minix3_inode));
  vnode_init(&vno->vnode, number, fs, &minix3_vnode_ops, &minix3_file_ops);
  return vno;
}

static struct vnode *minix3_vnode_get(struct minix3_fs *minix3, u32 number) {
  mutex_lock(&minix3->vnode_mtx);
  struct vnode *vno = vcache_find(&minix3->fs, number);
  if(vno != NULL) {
    mutex_unlock(&minix3->vnode_mtx);
    return vno;
  }

  number--;
  u32 inoblk = number / MINIX3_INODES_PER_BLOCK;
  u32 inooff = number % MINIX3_INODES_PER_BLOCK;
  struct blkbuf *bbuf = blkbuf_get(minix3->devno, get_inodetableblk(&minix3->sb) + inoblk); 
  blkbuf_sync(bbuf);

  struct minix3_inode *ino = (struct minix3_inode *)(bbuf->addr) + inooff;
  struct minix3_vnode *m3vno = minix3_vnode_new(&minix3->fs, number, ino);
  vno = &m3vno->vnode;
  blkbuf_release(bbuf);
  if(vcache_add(&minix3->fs, vno)) {
    //vcache is full
    minix3_vfree(vno);
    mutex_unlock(&minix3->vnode_mtx);
    return NULL;
  }

  mutex_unlock(&minix3->vnode_mtx);
  return vno;
}

static int ffz_byte(u8 byte) {
  int num = 0;
  if((byte & 0xf) == 0) {
    num += 4;
    byte >>= 4;
  }
  if((byte & 0x3) == 0) {
    num += 2;
    byte >>= 2;
  }
  if((byte & 0x1) == 0) {
    num += 1;
    byte >>= 1;
  }
  return num;
}

static u32 bitmap_get(struct minix3_fs *minix3, blkno_t startblk, blkno_t *pos, size_t nblocks) {
  u8 bits;
  u32 acc = 0;
  struct blkbuf *bbuf;
  size_t i;
  int first;
  for(i = 0; i < nblocks; i++){
    bbuf = blkbuf_get(minix3->devno, *pos++);
    blkbuf_sync(bbuf);
    for(u32 i=0; i< BITS_PER_BLOCK; i++) {
      if((bits = ((u8 *)bbuf->addr)[i]) != 0xff)
        goto exit;
      acc += 8;
    }
    blkbuf_release(bbuf);
    if(*pos >= startblk + nblocks)
      *pos = startblk;
  }
  return 0; //zero is error value

exit:
  first = ffz_byte(bits);
  ((u8 *)bbuf->addr)[i] |= (1 << (first-1));
  blkbuf_release(bbuf);
  return acc + first;
}

static ino_t minix3_inumber_get(struct minix3_fs *minix3) {
  mutex_lock(&minix3->imap_mtx);
  ino_t number = (ino_t)bitmap_get(minix3, get_inodemapblk(&minix3->sb), 
                   &(minix3->imap_search_pos), mblk_to_blk(minix3->sb.s_imap_blocks));
  mutex_unlock(&minix3->imap_mtx);
  return number;
}

static zone_t minix3_zone_get(struct minix3_fs *minix3) {
  mutex_lock(&minix3->zmap_mtx);
  zone_t zone = (zone_t)bitmap_get(minix3, get_zonemapblk(&minix3->sb), 
                   &(minix3->zmap_search_pos), mblk_to_blk(minix3->sb.s_zmap_blocks));
  mutex_unlock(&minix3->zmap_mtx);
  return zone;
}


static void bitmap_clear(struct minix3_fs *minix3, blkno_t start_blk, u32 num) {
  u32 offset_blks = (num-1) / BITS_PER_BLOCK;
  struct blkbuf *bbuf = blkbuf_get(minix3->devno, start_blk + offset_blks);
  blkbuf_sync(bbuf);
  ((u8*)bbuf->addr)[(num-1) << 3] |= 1 << ((num-1) & 7);
  blkbuf_markdirty(bbuf);
  blkbuf_release(bbuf);
}

static void minix3_inumber_free(struct minix3_fs *minix3, ino_t inode) {
  mutex_lock(&minix3->imap_mtx);
  if(inode == MINIX3_INVALID_INODE || minix3->sb.s_ninodes < inode) {
    mutex_unlock(&minix3->imap_mtx);
    return;
  }
  bitmap_clear(minix3, get_inodemapblk(&minix3->sb), (u32)inode);
  mutex_unlock(&minix3->imap_mtx);
}

static void minix3_zone_free(struct minix3_fs *minix3, zone_t zone) {
  mutex_lock(&minix3->zmap_mtx);
  if(zone == MINIX3_INVALID_ZONE || minix3->sb.s_zones < zone) {
    mutex_unlock(&minix3->zmap_mtx);
    return;
  }
  bitmap_clear(minix3, get_zonemapblk(&minix3->sb), (u32)zone);
  mutex_unlock(&minix3->zmap_mtx);
}

static int minix3_vnode_link_inc(struct vnode *vno) {
  struct minix3_vnode *m3vno = container_of(vno, struct minix3_vnode, vnode);
  if(m3vno->minix3.i_nlinks == MINIX3_MAX_LINK)
    return -1;
  m3vno->minix3.i_nlinks++;
  vnode_markdirty(vno);
  return 0;
}

static int minix3_vnode_truncate(struct vnode *vno, size_t size);

static void minix3_vnode_destroy(struct minix3_fs *minix3, struct vnode *vno) {
  vcache_remove(vno);
  minix3_vnode_truncate(vno, 0);
  minix3_inumber_free(minix3, vno->number);
  minix3_vfree(vno);
}

static void minix3_vnode_link_dec(struct minix3_fs *minix3, struct vnode *vno) {
  struct minix3_vnode *m3vno = container_of(vno, struct minix3_vnode, vnode);
  m3vno->minix3.i_nlinks--;
  if(m3vno->minix3.i_nlinks == 0)
    minix3_vnode_destroy(minix3, vno);
  else
    vnode_markdirty(vno);
}

/*
  vzone is virtual zone number.
  vzone starts from 0 but minix3 filesystem zone number starts from 1.
*/

static zone_t zone_vtop(struct minix3_vnode *m3vno, zone_t vzone, int is_write_access) {
  struct minix3_fs *minix3 = container_of(m3vno->vnode.fs, struct minix3_fs, fs);
  zone_t current_zone;
  int depth;
  int divisor;

  if(vzone < minix3->zone_boundary[0]) {
    if(is_write_access && m3vno->minix3.i_zone[vzone] == 0) {
      m3vno->minix3.i_zone[vzone] = minix3_zone_get(minix3);
      vnode_markdirty(&m3vno->vnode);
    }
    return m3vno->minix3.i_zone[vzone];
  } else if(vzone < minix3->zone_boundary[1]) {
    depth = 1;
    if(is_write_access && m3vno->minix3.i_zone[MINIX3_INDIRECT_ZONE] == 0) {
      m3vno->minix3.i_zone[MINIX3_INDIRECT_ZONE] = minix3_zone_get(minix3);
      vnode_markdirty(&m3vno->vnode);
    }
    current_zone = m3vno->minix3.i_zone[MINIX3_INDIRECT_ZONE];
  } else if(vzone < minix3->zone_boundary[2]) {
    depth = 2;
    if(is_write_access && m3vno->minix3.i_zone[MINIX3_DOUBLE_INDIRECT_ZONE] == 0) {
      m3vno->minix3.i_zone[MINIX3_DOUBLE_INDIRECT_ZONE] = minix3_zone_get(minix3);
      vnode_markdirty(&m3vno->vnode);
    }
    current_zone = m3vno->minix3.i_zone[MINIX3_DOUBLE_INDIRECT_ZONE];
  } else if(vzone < minix3->zone_boundary[3]) {
    depth = 3;
    if(is_write_access && m3vno->minix3.i_zone[MINIX3_TRIPLE_INDIRECT_ZONE] == 0) {
      m3vno->minix3.i_zone[MINIX3_TRIPLE_INDIRECT_ZONE] = minix3_zone_get(minix3);
      vnode_markdirty(&m3vno->vnode);
    }
    current_zone = m3vno->minix3.i_zone[MINIX3_TRIPLE_INDIRECT_ZONE];
  } else {
    //block number too large!
    puts("minix3fs: block number too large.");
    return 0;
  }

  vzone -= minix3->zone_boundary[depth-1];
  divisor = minix3->zone_divisor[depth-1];

  for(int i=0; i<depth; i++) {
    if(!is_write_access && current_zone == MINIX3_INVALID_ZONE)
      return MINIX3_INVALID_ZONE;
    u32 current_blk = zone_to_blk(&minix3->sb, current_zone);
    u32 indirect_index = vzone / minix3->zone_divisor[depth-1-i];
    u32 blk_offset = indirect_index * sizeof(zone_t) / BLOCKSIZE;
    struct blkbuf *bbuf = blkbuf_get(minix3->devno, current_blk + blk_offset);
    blkbuf_sync(bbuf);
    if(is_write_access && ((zone_t *)bbuf->addr)[indirect_index] == 0) {
      blkbuf_markdirty(bbuf);
      if((((zone_t *)bbuf->addr)[indirect_index] = minix3_zone_get(minix3)) == 0) {
        blkbuf_release(bbuf);
        return MINIX3_INVALID_ZONE;
      }
    }
    current_zone = ((zone_t *)bbuf->addr)[indirect_index];
    blkbuf_release(bbuf);
    vzone = vzone & (divisor-1);
  }

  return current_zone;
}

static int zone_truncate(struct minix3_vnode *m3vno, zone_t start, size_t count) {
  struct minix3_fs *minix3 = container_of(m3vno->vnode.fs, struct minix3_fs, fs);
  zone_t current_zone;
  int depth = 0;

  for(zone_t z = start+count-1; z >= start; z--) {
    if(z < minix3->zone_boundary[0]) {
      minix3_zone_free(minix3, m3vno->minix3.i_zone[z]);
      m3vno->minix3.i_zone[z] = 0;
      vnode_markdirty(&m3vno->vnode);
    } else if(z < minix3->zone_boundary[1]) {
      depth = 1;
      current_zone = m3vno->minix3.i_zone[MINIX3_INDIRECT_ZONE];
    } else if(z < minix3->zone_boundary[2]) {
      depth = 2;
      current_zone = m3vno->minix3.i_zone[MINIX3_DOUBLE_INDIRECT_ZONE];
    } else if(z < minix3->zone_boundary[3]) {
      depth = 3;
      current_zone = m3vno->minix3.i_zone[MINIX3_TRIPLE_INDIRECT_ZONE];
    } else {
      //block number too large!
      puts("minix3fs: block number too large.");
      return -1;
    }

    zone_t vzone = z - minix3->zone_boundary[depth-1];
    int divisor = minix3->zone_divisor[depth-1];

    for(int i=0; i<depth; i++) {
      if(current_zone == MINIX3_INVALID_ZONE)
        break;
      u32 current_blk = zone_to_blk(&minix3->sb, current_zone);
      u32 indirect_index = vzone / minix3->zone_divisor[depth-1-i];
      u32 blk_offset = indirect_index * sizeof(zone_t) / BLOCKSIZE;
      struct blkbuf *bbuf = blkbuf_get(minix3->devno, current_blk + blk_offset);
      blkbuf_sync(bbuf);
      if(i == depth-1) {
        minix3_zone_free(minix3, ((zone_t *)bbuf->addr)[indirect_index]);
        ((zone_t *)bbuf->addr)[indirect_index] = MINIX3_INVALID_ZONE;
        blkbuf_markdirty(bbuf);
      } else {
        current_zone = ((zone_t *)bbuf->addr)[indirect_index];
      }
      blkbuf_release(bbuf);
      vzone = vzone & (divisor-1);
    }
  }

  return 0;
}

static int minix3_vnode_truncate(struct vnode *vno, size_t size) {
  struct minix3_fs *minix3 = container_of(vno->fs, struct minix3_fs, fs);
  struct minix3_vnode *m3vno = container_of(vno, struct minix3_vnode, vnode);
  size_t allocated_zones = UPPER(m3vno->minix3.i_size, minix3->zone_size); 
  size_t needed_zones = UPPER(size, minix3->zone_size);
  
  if(allocated_zones > needed_zones) {
    size_t nzones = allocated_zones - needed_zones;
    return zone_truncate(m3vno, needed_zones, nzones);
  }

  return 0;
}

static int minix3_is_valid_sb(struct minix3_sb *sb) {
  if(sb->s_magic != MINIX3_SUPER_MAGIC)
    return 0;

  return 1;
}

static struct fs *minix3_mount(devno_t devno) {
  struct minix3_fs *minix3 = malloc(sizeof(struct minix3_fs));
  minix3->devno = devno;

  struct blkbuf *bbuf = blkbuf_get(devno, MINIX3_SUPERBLOCK);
  blkbuf_sync(bbuf);
  minix3->sb = *(struct minix3_sb *)(bbuf->addr);
  blkbuf_release(bbuf);

  minix3->fs.fs_ops = &minix3_fs_ops;
  minix3->zone_size = MBLOCKSIZE << minix3->sb.s_log_zone_size;
  minix3->blocks_in_zone = BLOCKS_PER_MBLOCK << minix3->sb.s_log_zone_size;
  minix3->zones_in_indirect_zone = minix3->zone_size / sizeof(zone_t);

  minix3->zone_divisor[0] = 1; 
  for(int i=1; i<MINIX3_INDIRECT_DEPTH; i++)
    minix3->zone_divisor[i] = minix3->zones_in_indirect_zone * minix3->zone_divisor[i-1];

  zone_t direct = MINIX3_INDIRECT_ZONE;
  zone_t indirect = minix3->zones_in_indirect_zone;
  zone_t dindirect = indirect * indirect;
  zone_t tindirect = indirect * indirect * indirect;

  minix3->zone_boundary[0] = direct;
  minix3->zone_boundary[1] = direct + indirect;
  minix3->zone_boundary[2] = direct + indirect + dindirect;
  minix3->zone_boundary[3] = direct + indirect + dindirect + tindirect;

  minix3->imap_search_pos = get_inodemapblk(&minix3->sb);
  minix3->zmap_search_pos = get_zonemapblk(&minix3->sb);

  mutex_init(&minix3->imap_mtx);
  mutex_init(&minix3->zmap_mtx);
  mutex_init(&minix3->vnode_mtx);

  if(!minix3_is_valid_sb(&minix3->sb)) {
    puts("minix3fs: bad superblock");
    free(minix3);
    return NULL;
  }    
  return &minix3->fs;
}

static struct vnode *minix3_getroot(struct fs *fs) {
  struct minix3_fs *minix3 = container_of(fs, struct minix3_fs, fs);
  return minix3_vnode_get(minix3, MINIX3_ROOT_INODE);
}

static int minix3_firstblk(struct minix3_vnode *m3vno, size_t file_off, int is_write_access) {
  struct minix3_fs *minix3 = container_of(m3vno->vnode.fs, struct minix3_fs, fs);
  zone_t vzone = file_off / minix3->zone_size;
  zone_t current_zone = zone_vtop(m3vno, vzone, is_write_access);

  if(current_zone == MINIX3_INVALID_ZONE)
    return -1;

  return zone_to_blk(&minix3->sb, current_zone); 
}

static int minix3_nextblk(struct minix3_vnode *m3vno, size_t offset, int prevblk, int is_write_access) {
  struct minix3_fs *minix3 = container_of(m3vno->vnode.fs, struct minix3_fs, fs);
  if(prevblk % minix3->blocks_in_zone != minix3->blocks_in_zone-1) {
    return prevblk+1;
  } else {
    //go over a zone boundary
    return minix3_firstblk(m3vno, offset, is_write_access);
  }
}

static struct vnode *minix3_dentop(struct vnode *vnode, const char *name, int op, ino_t number, int *status) {
  struct minix3_fs *minix3 = container_of(vnode->fs, struct minix3_fs, fs);
  struct minix3_vnode *m3vno = container_of(vnode, struct minix3_vnode, vnode);
  
  if((m3vno->minix3.i_mode & S_IFMT) != S_IFDIR) {
    if(status != NULL)
      *status = LOOKUP_ERROR;
    return NULL;
  }

  int is_write_access;
  switch(op) {
  case OP_LOOKUP:
  case OP_EMPTY_CHECK:
    is_write_access = 0;
    break;
  case OP_REMOVE:
  case OP_ADD:
    is_write_access = 1;
    break;
  default:
    return NULL;
  }

  zone_t vzone = 0;
  zone_t current_zone = zone_vtop(m3vno, vzone, is_write_access);
  size_t remain = m3vno->minix3.i_size;
  struct blkbuf *bbuf = NULL;
  struct vnode *result = NULL;
  if(op == OP_ADD && remain == 0) {
    remain += minix3->zone_size;
    m3vno->minix3.i_size += minix3->zone_size;
    vnode_markdirty(vnode);
  }

  while(remain > 0) {
    if(current_zone == 0) {
      if(status != NULL)
        *status = LOOKUP_ERROR;
      return NULL;
    }

    u32 zone_firstblk = zone_to_blk(&minix3->sb, current_zone);
    for(u32 blk = 0; remain > 0 && blk < minix3->blocks_in_zone; blk++) {
      bbuf = blkbuf_get(minix3->devno, zone_firstblk + blk);
      blkbuf_sync(bbuf);
      for(u32 i=0; i < MINIX3_DENTS_PER_BLOCK && remain > 0; 
            i++, remain -= sizeof(struct minix3_dent)) {
        struct minix3_dent *dent = &(((struct minix3_dent *)bbuf->addr)[i]);
        switch(op) {
        case OP_LOOKUP:
          if(dent->inode == 0)
            break;
          if(strncmp(name, dent->name, MINIX3_MAX_NAME_LEN) == 0) {
            result = minix3_vnode_get(minix3, dent->inode);
            if(status != NULL)
              *status = LOOKUP_FOUND;
            goto exit;
          }
          break;
        case OP_EMPTY_CHECK:
          if(dent->inode == 0)
            break;
          if(strcmp(".", dent->name) && strcmp("..", dent->name)) {
            result = minix3_vnode_get(minix3, dent->inode);
            goto exit;
          }
          break;
        case OP_REMOVE:
          if(dent->inode == 0)
            break;
          if(strncmp(name, dent->name, MINIX3_MAX_NAME_LEN) == 0) {
            dent->inode = MINIX3_INVALID_INODE;
            blkbuf_markdirty(bbuf);
            result = vnode;
            goto exit;
          }
          break;
        case OP_ADD:
          if(dent->inode != 0)
            break;
          strncpy(dent->name, name, MINIX3_MAX_NAME_LEN);
          dent->inode = number;
          blkbuf_markdirty(bbuf);
          result = vnode;
          goto exit;
        }
      }
      blkbuf_release(bbuf);
    }

    current_zone = zone_vtop(m3vno, ++vzone, is_write_access);
    if(op == OP_ADD && remain == 0) {
      remain += minix3->zone_size;
      m3vno->minix3.i_size += minix3->zone_size;
      vnode_markdirty(vnode);
    }
  }

  if(status != NULL)
    *status = LOOKUP_NOTFOUND;

exit:
  if(bbuf != NULL)
    blkbuf_release(bbuf);

  return result;
}


int minix3_read(struct file *f, void *buf, size_t count) {
  struct vnode *vno = (struct vnode *)f->data;
  struct minix3_vnode *m3vno = container_of(vno, struct minix3_vnode, vnode);
  struct minix3_fs *minix3 = container_of(m3vno->vnode.fs, struct minix3_fs, fs);

  vnode_lock(vno);

  if(f->offset < 0) {
    vnode_unlock(vno);
    return -1;
  }

  u32 offset = (u32)f->offset;
  u32 tail = MIN(count + offset, m3vno->minix3.i_size);
  u32 remain = tail - offset;

  if(tail <= offset) {
    vnode_unlock(vno);
    return 0;
  }

  u32 pos = offset;
  for(int blkno = minix3_firstblk(m3vno, pos, 0);
        blkno > 0 && remain > 0; ) {
    struct blkbuf *bbuf = blkbuf_get(minix3->devno, blkno);
    blkbuf_sync(bbuf);
    u32 inblk_off = pos % BLOCKSIZE;
    u32 copylen = MIN(BLOCKSIZE - inblk_off, remain);
    memcpy(buf, bbuf->addr + inblk_off, copylen);
    blkbuf_release(bbuf);
    buf += copylen;
    pos += copylen;
    remain -= copylen;
    
    blkno = minix3_nextblk(m3vno, pos, blkno, 0);
  }

  u32 read_bytes = (tail - offset) - remain;
  f->offset += read_bytes;

  vnode_unlock(vno);
  return read_bytes;
}

int minix3_write(struct file *f, const void *buf, size_t count) {
  struct vnode *vno = (struct vnode *)f->data;
  struct minix3_vnode *m3vno = container_of(vno, struct minix3_vnode, vnode);
  struct minix3_fs *minix3 = container_of(m3vno->vnode.fs, struct minix3_fs, fs);

  vnode_lock(vno);

  if(f->offset < 0) {
    vnode_unlock(vno);
    return -1;
  }
  u32 offset = (u32)f->offset;
  u32 tail = MIN(count + offset, m3vno->minix3.i_size);
  u32 remain = tail - offset;

  if(tail <= offset) {
    vnode_unlock(vno);
    return 0;
  }

  u32 pos = offset;
  for(int blkno = minix3_firstblk(m3vno, pos, 1);
        blkno > 0 && remain > 0; ) {
    struct blkbuf *bbuf = blkbuf_get(minix3->devno, blkno);
    u32 inblk_off = pos % BLOCKSIZE;
    u32 copylen = MIN(BLOCKSIZE - inblk_off, remain);
    if(copylen != BLOCKSIZE) 
      blkbuf_sync(bbuf);
    memcpy(bbuf->addr + inblk_off, buf, copylen);
    blkbuf_markdirty(bbuf);
    blkbuf_release(bbuf);
    buf += copylen;
    pos += copylen;
    remain -= copylen;
    
    blkno = minix3_nextblk(m3vno, pos, blkno, 1);
  }

  u32 wrote_bytes = (tail - offset) - remain;
  f->offset += wrote_bytes;
  if(f->offset > m3vno->minix3.i_size) {
    m3vno->minix3.i_size = f->offset;
    vnode_markdirty(vno);
  }

  vnode_unlock(vno);
  return wrote_bytes;
}

int minix3_lseek(struct file *f, off_t offset, int whence) {
  struct vnode *vno = (struct vnode *)f->data;
  struct minix3_vnode *m3vno = container_of(vno, struct minix3_vnode, vnode);
  switch(whence) {
  case SEEK_SET:
    f->offset = offset;
    break;
  case SEEK_CUR:
    f->offset += offset;
    break;
  case SEEK_END:
    f->offset = m3vno->minix3.i_size;
    break;
  default:
    return -1;
  }
 
  return 0;
}

int minix3_close(struct file *f) {
  minix3_sync(f);
  return 0;
}

int minix3_sync(struct file *f) {
  struct vnode *vno = (struct vnode *)f->data;
  minix3_vsync(vno);
  return 0;
}

int minix3_truncate(struct file *f, size_t size) {
  struct vnode *vno = (struct vnode *)f->data;
  return minix3_vnode_truncate(vno, size);
}
  
int minix3_getdents(struct file *f, struct dirent *dirp, size_t count) {
  struct vnode *vno = (struct vnode *)f->data;
  struct minix3_vnode *m3vno = container_of(vno, struct minix3_vnode, vnode);
  struct minix3_fs *minix3 = container_of(m3vno->vnode.fs, struct minix3_fs, fs);

  vnode_lock(vno);

  if(f->offset < 0) {
    vnode_unlock(vno);
    return -1;
  }

  u32 offset = (u32)f->offset;
  u32 tail = m3vno->minix3.i_size;
  u32 remain = tail - offset;

  if(tail <= offset) {
    vnode_unlock(vno);
    return 0;
  }

  size_t buf_remain = count;

  u32 pos = offset;
  for(int blkno = minix3_firstblk(m3vno, pos, 0);
        blkno > 0 && remain > 0 && buf_remain >= sizeof(struct dirent); ) {
    struct blkbuf *bbuf = blkbuf_get(minix3->devno, blkno);
    blkbuf_sync(bbuf);
    u32 inblk_off = pos % BLOCKSIZE;
    u32 inblk_remain = BLOCKSIZE - inblk_off;
    struct minix3_dent *dent = (struct minix3_dent *)(bbuf->addr + inblk_off);

    while(inblk_remain >= MINIX3_DENT_SIZE
              && buf_remain >= sizeof(struct dirent)) {
      if(dent->inode != MINIX3_INVALID_INODE) {
        dirp->d_vno = dent->inode;
        strncpy(dirp->d_name, dent->name, MINIX3_MAX_NAME_LEN+1);
        dirp++;
        buf_remain -= sizeof(struct dirent);
      }
      dent++;
      inblk_remain -= MINIX3_DENT_SIZE;
    }

    blkbuf_release(bbuf);
    pos += MIN(BLOCKSIZE - inblk_off, remain);
    remain -= MIN(BLOCKSIZE - inblk_off, remain);
    
    blkno = minix3_nextblk(m3vno, pos, blkno, 0);
  }

  u32 read_bytes = (tail - offset) - remain;
  f->offset += read_bytes;

  vnode_unlock(vno);
  return count - buf_remain;
}

int minix3_lookup(struct vnode *vno, const char *name, struct vnode **found) {
  int status;
  struct vnode *foundvno = minix3_dentop(vno, name, OP_LOOKUP, 0, &status);
  if(found != NULL)
    *found = foundvno;
  else if(foundvno != NULL)
    vnode_release(foundvno);
  return status;
}

int minix3_mknod(struct vnode *parent, const char *name, int mode, devno_t devno) {
  struct minix3_fs *minix3 = container_of(parent->fs, struct minix3_fs, fs);
  struct minix3_inode inode;
  bzero(&inode, sizeof(struct minix3_inode));

  inode.i_mode = mode;
  switch(mode & S_IFMT) {
  case S_IFREG:
  case S_IFDIR:
    break;
  case S_IFBLK:
    inode.i_zone[0] = devno;
    break;
  case S_IFCHR:
    inode.i_zone[0] = devno;
    break;
  default:
    return -1;
  }

  ino_t number = minix3_inumber_get(minix3);
  if(number == MINIX3_INVALID_INODE)
    return -1;
  struct vnode *vno = minix3_vnode_get(minix3, number);
  if(vno == NULL)
    return -1;

  struct minix3_vnode *m3vno = container_of(vno, struct minix3_vnode, vnode);
  m3vno->minix3 = inode;
  vnode_markdirty(vno);

  if(minix3_link(parent, name, vno)) {
    minix3_vnode_destroy(minix3, vno);
    return -1;
  }

  switch(mode & S_IFMT) {
  case S_IFDIR:
    minix3_link(vno, ".", vno);
    minix3_link(vno, "..", parent);
    break;
  }

  return 0;
}

int minix3_link(struct vnode *parent, const char *name, struct vnode *vno) {
  if(minix3_dentop(parent, name, OP_ADD, vno->number, NULL) == NULL)
    return -1;

  return minix3_vnode_link_inc(vno);
}

int minix3_unlink(struct vnode *parent, const char *name, struct vnode *vno) {
  struct minix3_vnode *m3vno = container_of(vno, struct minix3_vnode, vnode);
  if(!strcmp(name, ".") || !strcmp(name, ".."))
    return -1;
  if((m3vno->minix3.i_mode & S_IFMT) == S_IFDIR) {
    struct vnode *found = minix3_dentop(vno, name, OP_EMPTY_CHECK, 0, NULL);
    if(found != NULL) {
      vnode_release(found);
      return -1;
    }
    minix3_vnode_link_dec(container_of(vno->fs, struct minix3_fs, fs), vno); // .
  }
  if(minix3_dentop(parent, name, OP_REMOVE, 0, NULL) == NULL)
    return -1;
  
  struct minix3_fs *minix3 = container_of(vno->fs, struct minix3_fs, fs);
  minix3_vnode_link_dec(minix3, vno);
  return 0;
}

int minix3_stat(struct vnode *vno, struct stat *buf) {
  struct minix3_vnode *m3vno = container_of(vno, struct minix3_vnode, vnode);
  struct minix3_fs *minix3 = container_of(vno->fs, struct minix3_fs, fs);

  bzero(buf, sizeof(struct stat));
  buf->st_dev = minix3->devno;
  buf->st_mode =  m3vno->minix3.i_mode & S_IFMT;
  buf->st_size = m3vno->minix3.i_size;

  return 0;
}

void minix3_vfree(struct vnode *vno) {
  struct minix3_vnode *m3vno = container_of(vno, struct minix3_vnode, vnode);
  free(m3vno); 
}

void minix3_vsync(struct vnode *vno) {
  if(!(vno->flags & V_DIRTY))
    return;

  struct minix3_vnode *m3vno = container_of(vno, struct minix3_vnode, vnode);
  struct minix3_fs *minix3 = container_of(m3vno->vnode.fs, struct minix3_fs, fs);
  ino_t number = (ino_t)vno->number;
  u32 inoblk = number / MINIX3_INODES_PER_BLOCK;
  u32 inooff = number % MINIX3_INODES_PER_BLOCK;
  struct blkbuf *bbuf = blkbuf_get(minix3->devno, get_inodetableblk(&minix3->sb) + inoblk); 
  blkbuf_sync(bbuf);
  struct minix3_inode *ino = (struct minix3_inode *)(bbuf->addr) + inooff;
  *ino = m3vno->minix3;
  blkbuf_markdirty(bbuf);
  blkbuf_sync(bbuf);
  blkbuf_release(bbuf);

  vno->flags &= ~V_DIRTY;
}

