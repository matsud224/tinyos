#include <kern/kernlib.h>
#include <kern/fs.h>
#include <kern/blkdev.h>

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


static const struct fstype_ops minix3_fstype_ops = {
  .mount = minix3_mount,
};

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

#define MINIX3_NAME_MAX       60
struct minix3_dent {
  ino_t inode;
  char name[MINIX3_NAME_MAX];
} PACKED;

#define MINIX3_INDIRECT_DEPTH 3
struct minix3_fs {
  devno_t devno;
  struct minix3_sb sb;
  struct fs fs;
  u32 zone_size;
  u32 blocks_in_zone;
  u32 zones_in_indirect_zone;
  zone_t zone_boundary[MINIX3_INDIRECT_DEPTH+1];
  u32 zone_divisor[MINIX3_INDIRECT_DEPTH];
  blkno_t zmap_search_pos;
  blkno_t zmap_search_pos;
};

struct minix3_vnode {
  struct minix3_inode minix3;
  struct vnode vnode;
};


#define MINIX3_BOOT 0
#define MINIX3_SUPERBLOCK 1

#define MINIX_BLOCK_SIZE_BITS 10
#define MINIX_BLOCK_SIZE     (1 << MINIX_BLOCK_SIZE_BITS)

#define MINIX_MAX_INODES     65535

#define MINIX2_INODES_PER_BLOCK ((MINIX_BLOCK_SIZE)/(sizeof (struct minix2_inode)))
#define MINIX3_DENTS_PER_BLOCK (MINIX_BLOCK_SIZE / sizeof(struct minix3_dent))

#define MINIX3_INDIRECT_ZONE 7
#define MINIX3_DOUBLE_INDIRECT_ZONE 8
#define MINIX3_TRIPLE_INDIRECT_ZONE 9

#define MINIX3_MAX_FILE_NAME 60

#define MINIX3_SUPER_MAGIC   0x4d5a          /* minix V3 fs (60 char names) */

#define INODE2_SIZE (sizeof(struct minix2_inode))
#define BITS_PER_BLOCK (MINIX_BLOCK_SIZE << 3)

#define UPPER(size,n) ((size+((n)-1))/(n))

#define BLOCKS_IN_MINIX_BLOCK (MINIX_BLOCK_SIZE / BLOCKSIZE)

#define get_inodemapblk(sb) (2)
#define get_zonemapblk(sb) (2 + (sb)->s_imap_blocks)
#define get_inodetableblk(sb) (2 + (sb)->s_imap_blocks + (sb)->s_zmap_blocks)
#define get_datazoneblk(sb) ((sb)->s_firstdatazone)

#define minixblk_to_blk(b) ((b)*BLOCKS_IN_MINIX_BLOCK)

#define S_IFMT  00170000
#define S_IFSOCK 0140000
#define S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000

FS_INIT void minix3_init() {
  fstype_register("minix3", &minix3_fstype_ops);
}

static struct vnode *minix3_vnode_new(struct fs *fs, u32 number, struct minix3_inode *inode) {
  struct minix3_vnode *vno = malloc(sizeof(struct minix3_vnode));
  memcpy(vno->minix3, inode, sizeof(struct minix3_inode));
  vno->vnode.fs = fs;
  vno->vnode.ops = &minix3_vnode_ops;
  vno->vnode.number = number;
  vno->vnode.type = V_REGULAR;
  return &(vno->vnode);
}

static struct minix3_vnode *minix3_vnode_get(struct fs *fs, u32 number) {
  struct minix3_fs *minix3 = malloc(sizeof(struct minix3_fs));
  u32 inoblk = number / (MINIX2_INODES_PER_BLOCK / BLOCKS_IN_MINIX_BLOCK);
  u32 inooff = number % (MINIX2_INODES_PER_BLOCK / BLOCKS_IN_MINIX_BLOCK);
  struct blkbuf *bbuf = blkbuf_get(minix3->devno, get_inodetableblk(sb) + inoblk; 
  blkbuf_sync(bbuf);

  struct minix3_inode *ino = (struct minix3_inode *)(bbuf->addr) + inooff;
  struct minix3_vnode *m3vno = minix3_vnode_new(fs, number, ino);
  blkbuf_release(bbuf);
  return &(m3vno->vnode);
}

static void minix3_vnode_free(struct vnode *vno) {
}

static zone_t zone_vtop(struct minix3_vnode *m3vno, zone_t vzone, int is_write_access) {
  struct minix3_fs *minix3 = container_of(&(m3vno->vnode.fs), struct minix3_fs, fs);
  struct minix3_sb *sb = &minix3->sb;
  zone_t current_zone;
  int depth;

  if(vzone < zone_boundary[0]) {
    if(is_write_access && m3vno->minix3.i_zone[vzone] == 0)
      m3vno->minix3.i_zone[vzone] = minix3_zone_get(minix3);
    return m3vno->minix3.i_zone[vzone];
  } else if(vzone < minix3->zone_boundary[1]) {
    depth = 1;
    if(is_write_access && m3vno->minix3.i_zone[MINIX3_INDIRECT_ZONE] == 0)
      m3vno->minix3.i_zone[MINIX3_INDIRECT_ZONE] = minix3_zone_get(minix3);
    current_zone = m3vno->minix3.i_zone[MINIX3_INDIRECT_ZONE];
  } else if(vzone < minix3->zone_boundary[2]) {
    depth = 2;
    if(is_write_access && m3vno->minix3.i_zone[MINIX3_DOUBLE_INDIRECT_ZONE] == 0)
      m3vno->minix3.i_zone[MINIX3_DOUBLE_INDIRECT_ZONE] = minix3_zone_get(minix3);
    current_zone = m3vno->minix3.i_zone[MINIX3_DOUBLE_INDIRECT_ZONE];
  } else if(vzone < minix3->zone_boundary[3]) {
    depth = 3;
    if(is_write_access && m3vno->minix3.i_zone[MINIX3_TRIPLE_INDIRECT_ZONE] == 0)
      m3vno->minix3.i_zone[MINIX3_TRIPLE_INDIRECT_ZONE] = minix3_zone_get(minix3);
    current_zone = m3vno->minix3.i_zone[MINIX3_TRIPLE_INDIRECT_ZONE];
  } else {
    //block number too large!
    puts("minix3fs: block number too large.");
    return 0;
  }

  vzone -= minix3->zone_boundary[depth-1];
  divisor = minix3->zone_divisor[depth-1];

  for(int i=0; i<depth; i++) {
    if(!is_write_access && current_zone == 0)
      return 0;
    u32 current_blk = get_zonefirstblk(sb, current_zone);
    u32 indirect_index = vzone / minix3->zone_divisor[depth-1-i];
    u32 blk_offset = indirect_index * sizeof(zone_t) / BLOCKSIZE;
    struct blkbuf *bbuf = blkbuf_get(minix3->devno, current_blk + blk_offset);
    blkbuf_sync(bbuf);
    if(is_write_access && ((zone_t *)bbuf->addr)[indirect_index] == 0) {
      blkbuf_markdirty(bbuf);
      if((((zone_t *)bbuf->addr)[indirect_index] = minix3_zone_get(minix3)) == 0) {
        blkbuf_release(bbuf);
        return 0;
      }
    }
    current_zone = ((zone_t *)bbuf->addr)[indirect_index];
    blkbuf_release(bbuf);
    vzone = vzone & (divisor-1);
  }

  return current_zone;
}

static u32 bitmap_get(struct minix3_fs *minix3, blkno_t startblk, blkno_t *pos, size_t nblocks) {
  u8 bits;
  u32 acc = 0;
  struct blkbuf *bbuf;
  size_t i;
  for(i = 0; i < nblocks; i++){
    bbuf = blkbuf_get(minix3->devno, *pos++);
    blkbuf_sync(bbuf);
    for(u32 i=0; i<BITS_PER_BLOCK / BLOCKS_IN_MINIX_BLOCK; i++) {
      if((bits = ((u8 *)bbuf->addr)[i]) != ~0)
        goto exit;
      acc += 8;
    }
    blkbuf_release(bbuf);
    if(pos >= startblk + nblocks)
      pos = startblk;
  }
  return 0; //zero is error value

exit:
  for(int i=0; i<8; i++, bits >>= 1)
    if(bits & 1) {
      ((u8 *)bbuf->addr)[i] |= (i << 1);
      blkbuf_release(bbuf);
      return acc + i;
    }

  blkbuf_release(bbuf);
  return 0;
}

static ino_t minix3_inode_get(struct minix3_fs *minix3) {
  if(minix3->sb.s_ninodes < inode)
    return 0;

  return (ino_t)search_bitmap(minix3, minixblk_to_blk(minixblk_to_blk(get_inodemapblk(sb))), 
                   &(minix3->sb.imap_search_pos), minixblk_to_blk(minix3->sb.s_imap_blocks));
}

static zone_t minix3_zone_get(struct minix3_fs *minix3) {
  if(minix3->sb.s_s_zones < zone)
    return 0;
  return (zone_t)search_bitmap(minix3, minixblk_to_blk(minixblk_to_blk(get_zonemapblk(sb))), 
                   &(minix3->sb.zmap_search_pos), minixblk_to_blk(minix3->sb.s_zmap_blocks));
}


static void bitmap_clear(struct minix3_fs *minix3, blkno_t start_blk, u32 num) {
  u32 offset_blks = num / (BITS_PER_BLOCK / BLOCKS_IN_MINIX_BLOCK);
  struct blkbuf *bbuf = blkbuf_get(minix3->devno, start_blk + offset_blks);
  blkbuf_sync(bbuf);
  bbuf->addr[num << 3] |= 1 << (num & 7);
  blkbuf_markdirty(bbuf);
  blkbuf_release(bbuf);
}

static void minix3_inode_free(struct minix3_fs *minix3, ino_t inode) {
  if(minix3->sb.s_ninodes < inode)
    return;
  bitmap_clear(minix3, minixblk_to_blk(get_inodemapblk(&minix3->sb)), (u32)inode);
}

static void minix3_zone_free(struct minix3_fs *minix3, zone_t zone) {
  if(minix3->sb.s_s_zones < zone)
    return;
  bitmap_clear(minix3, get_zonemapblk(&minix3->sb), (u32)zone);
}

static int zone_truncate(struct minix3_vnode *m3vno, zone_t start, size_t count) {
  struct minix3_fs *minix3 = container_of(&(m3vno->vnode.fs), struct minix3_fs, fs);
  struct minix3_sb *sb = &minix3->sb;
  zone_t current_zone;
  int depth;

  for(zone_t z = start+count-1; z >= start; z--) {
    if(vzone < zone_boundary[0]) {
      minix3_zone_free(minix3, m3vno->minix3.i_zone);
      m3vno->minix3.i_zone[z] = 0;
    } else if(vzone < minix3->zone_boundary[1]) {
      depth = 1;
      current_zone = m3vno->minix3.i_zone[MINIX3_INDIRECT_ZONE];
    } else if(vzone < minix3->zone_boundary[2]) {
      depth = 2;
      current_zone = m3vno->minix3.i_zone[MINIX3_DOUBLE_INDIRECT_ZONE];
    } else if(vzone < minix3->zone_boundary[3]) {
      depth = 3;
      current_zone = m3vno->minix3.i_zone[MINIX3_TRIPLE_INDIRECT_ZONE];
    } else {
      //block number too large!
      puts("minix3fs: block number too large.");
      return -1;
    }

    vzone -= minix3->zone_boundary[depth-1];
    divisor = minix3->zone_divisor[depth-1];

    for(int i=0; i<depth; i++) {
      if(current_zone == 0)
        break;
      u32 current_blk = get_zonefirstblk(sb, current_zone);
      u32 indirect_index = vzone / minix3->zone_divisor[depth-1-i];
      u32 blk_offset = indirect_index * sizeof(zone_t) / BLOCKSIZE;
      struct blkbuf *bbuf = blkbuf_get(minix3->devno, current_blk + blk_offset);
      blkbuf_sync(bbuf);
      if(i == depth-1) {
        minix3_zone_free(((zone_t *)bbuf->addr)[indirect_index]);
        ((zone_t *)bbuf->addr)[indirect_index] = 0;
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

static int minix3_truncate(struct vnode *vno, size_t size) {
  struct minix3_fs *minix3 = container_of(vnode->fs, struct minix3_fs, fs);
  struct minix3_vnode *m3vno = container_of(vnode, struct minix3_vnode, minix);
  size_t allocated_zones = UPPER(m3vno->minix3.i_size, minix3->zone_size); 
  size_t needed_zones = UPPER(size, minix3->zone_size);
  
  if(allocated_zones > needed_zones) {
    size_t nzones = allocated_zones - needed_zones;
    return zone_truncate(minix3, needed_zones, nzones);
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

  struct blkdev_buf *bbuf = blkbuf_get(devno, minixblk_to_blk(MINIX3_SUPERBLOCK));
  blkbuf_sync(bbuf);
  minix3->sb = *(struct minix3_sb *)(bbuf->addr);
  blkbuf_release(bbuf);

  minix3->fs.fs_ops = &minix3_fs_ops;
  minix3->fs.file_ops = &minix3_file_ops;

  minix3->zone_size = MINIX_BLOCK_SIZE << minix3->sb.s_log_zone_size;
  minix3->blocks_in_zone = BLOCKS_IN_MINIX_BLOCK << minix3->sb.s_log_zone_size;
  minix3->zones_in_indirect_zone = minix3->zone_size / sizeof(zone_t);

  minix3->zone_divisor[0] = 1; 
  for(int i=1; i<MINIX3_INDIRECT_DEPTH; i++)
    minix3->zone_divisor[i] = minix3->zones_in_indirect_zone * minix3->zone_divisor[i-1];

  zone_t direct = MINIX2_INDIRECT_ZONE;
  zone_t indirect = minix3->zones_in_indirect_zone;
  zone_t dindirect = indirect * indirect;
  zone_t tindirect = indirect * indirect * indirect;

  minix3->zone_boundary[0] = direct;
  minix3->zone_boundary[1] = direct + indirect;
  minix3->zone_boundary[2] = direct + indirect + dindirect;
  minix3->zone_boundary[3] = direct + indirect + dindirect + tindirect;

  minix3->imap_search_pos = get_inodemapblk(sb);
  minix3->zmap_search_pos = get_zonemapblk(sb);

  if(!minix3_is_valid_sb(&minix3->sb)) {
    free(minix3);
    return NULL;
  }    
  return &minix3->fs;
}

static struct vnode *minix3_getroot(struct fs *fs) {
  struct minix3_fs *minix3 = container_of(fs, struct minix3_fs, fs);
  return minix3_vnode_get(&minix3->sb, 1);
}

static struct vnode *minix3_dentop(struct vnode *vnode, const char *name, int op, ino_t number) {
  struct minix3_fs *minix3 = container_of(vnode->fs, struct minix3_fs, fs);
  struct minix3_vnode *m3vno = container_of(vnode, struct minix3_vnode, minix);
  
  if((m3vno->minix.i_mode & S_IFMT) != S_IFDIR)
    return NULL;


  int is_write_access;
  switch(op) {
  case OP_LOOKUP:
    is_write_access = 0;
    break;
  case OP_UNLINK:
  case OP_ADD:
    is_write_access = 1;
    break;
  }

  zone_t vzone = 0;
  zone_t current_zone = zone_vtop(minix3, vzone, is_write_access);
  size_t remain = m3vno->minix.i_size;
  struct blkbuf *bbuf = NULL;
  struct vnode *result = NULL;

  if(op == OP_ADD && remain == 0) {
    remain += minix3->zone_size;
    m3vno->minix3.i_size += minix3->zone_size;
  }

  while(remain > 0) {
    if(current_zone == 0)
      return NULL;

    u32 zone_firstblk = getfirstzoneblk(&minix3->sb, current_zone); 
    for(u32 blk = 0; remain > 0 && blk < minix3->blocks_in_zone; blk++) {
      bbuf = blkbuf_get(devno, zone_firstblk + blk);
      blkbuf_sync(bbuf);
      for(u32 i=0; i < MINIX3_DENTS_PER_BLOCK / BLOCKS_PER_MINIX_BLOCK && remain > 0; 
            i++, remain -= sizeof(struct minix3_dent)) {
        struct minix3_dent *dent = ((struct minix3_dent *)buf->addr)[i];
        switch(op) {
        case OP_LOOKUP:
          if(dent->inode == 0)
            break;
          if(strncmp(name, dent->name, MINIX3_NAME_MAX) == 0) {
            result = minix3_vnode_get(dent->inode);
            goto exit;
          }
          break;
        case OP_UNLINK:
          if(dent->inode == 0)
            break;
          if(strncmp(name, dent->name, MINIX3_NAME_MAX) == 0) {
            struct vnode *vno0 = minix3_vnode_get(dent->inode);
            vno->i_nlinks--;
            if(i_nlinks == 0) {
              minix3_truncate(vno0, 0);
              minix3_vnode_free(vno0);
              minix3_inode_free(dent->inode);
            }
            dent->inode = 0;
            result = vno;
            goto exit;
          }
          break;
        case OP_ADD:
          if(dent->inode != 0)
            break;
          strncpy(dent->name, name, MINIX3_NAME_MAX);
          dent->inode = number;
          result = vno;
          goto exit;
        }
      }
      blkbuf_release(bbuf);
    }

    current_zone = zone_vtop(minix3, ++vzone, is_write_access);
    if(op == OP_ADD && remain == 0) {
      remain += minix3->zone_size;
      m3vno->minix3.i_size += minix3->zone_size;
    }
  }


exit:
  if(bbuf != NULL)
    blkbuf_release(bbuf);

  return result;
}


int minix3_read(struct file *f, void *buf, size_t count) {
  struct vnode *vno = (struct vnode *)f->data;
  struct minix3_vnode *m3vno = container_of(vno, struct minix3_vnode, vnode);

  if(f->offset < 0)
    return -1;
  u32 offset = (u32)f->offset;
  u32 tail = MIN(count + offset, m3vno->minix.i_size);
  u32 remain = tail - offset;

  if(tail <= offset)
    return 0;

  struct minix3_fs *minix3 = container_of(&(m3vno->vnode.fs), struct minix3_fs, fs);

  zone_t vzone = offset / minix3->zone_size;
  zone_t current_zone = zone_vtop(minix3, vzone, 0);
  u32 in_zone_off = offset & (minix3->zone_size-1);
  while(remain > 0) {
    if(current_zone == 0)
      break;
    u32 in_blk_off = in_zone_off & (BLOCKSIZE-1);
    u32 zone_firstblk = getfirstzoneblk(&minix3->sb, current_zone); 
    for(u32 blk = in_zone_off / BLOCKSIZE; remain > 0 && blk < minix3->blocks_in_zone; blk++) {
      struct blkbuf *bbuf = blkbuf_get(devno, zone_firstblk + blk);
      blkbuf_sync(bbuf);
      u32 copylen = MIN(BLOCKSIZE - in_blk_off, remain);
      memcpy(buf, bbuf->addr + in_blk_off, copylen);
      blkbuf_release(bbuf);
      buf += copylen;
      remain -= copylen;
      in_blk_off = 0;
    }

    current_zone = zone_vtop(minix3, ++vzone, 0);
    in_zone_off = 0;
  }

  u32 read_bytes = (tail - offset) - remain;
  f->offset += read_bytes;
  return read_bytes;
}

int minix3_write(struct file *f, const void *buf, size_t count) {
  struct vnode *vno = (struct vnode *)f->data;
  struct minix3_vnode *m3vno = container_of(vno, struct minix3_vnode, vnode);

  if(f->offset < 0)
    return -1;
  u32 offset = (u32)f->offset;
  u32 tail = MIN(count + offset, m3vno->minix.i_size);
  u32 remain = tail - offset;

  if(tail <= offset)
    return 0;

  struct minix3_fs *minix3 = container_of(&(m3vno->vnode.fs), struct minix3_fs, fs);

  zone_t vzone = offset / minix3->zone_size;
  zone_t current_zone = zone_vtop(minix3, vzone, 1);
  u32 in_zone_off = offset & (minix3->zone_size-1);
  while(remain > 0) {
    if(current_zone == 0)
      break;
    u32 in_blk_off = in_zone_off & (BLOCKSIZE-1);
    u32 zone_firstblk = getfirstzoneblk(&minix3->sb, current_zone); 
    for(u32 blk = in_zone_off / BLOCKSIZE; remain > 0 && blk < minix3->blocks_in_zone; blk++) {
      struct blkbuf *bbuf = blkbuf_get(devno, zone_firstblk + blk);
      u32 copylen = MIN(BLOCKSIZE - in_blk_off, remain);
      if(copylen != BLOCKSIZE) 
        blkbuf_sync(bbuf);
      memcpy(bbuf->addr + in_blk_off, buf, copylen);
      blkbuf_markdirty(bbuf);
      blkbuf_release(bbuf);
      buf += copylen;
      remain -= copylen;
      in_blk_off = 0;
    }

    current_zone = zone_vtop(minix3, ++vzone, 1);
    in_clus_off = 0;
  }

  u32 wrote_bytes = (tail - offset) - remain;
  f->offset += wrote_bytes;
  if(f->offset > m3vno->minix3.i_size)
    m3vno->minix3.i_size = f->offset;
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
  int retval = s->ops->close(s->pcb);

  mutex_lock(&socklist_mtx);
  list_remove(&s->link);
  mutex_unlock(&socklist_mtx);

  return retval;
}

int minix3_sync(struct file *f) {
  return 0;
}

struct vnode *minix3_create(struct vnode *vno, const char *name) {
  if(minix3_dentop(vno, name, OP_ADD, 0) == NULL)
    return -1;
  else
    return 0;
}

int minix3_mknod(struct vnode *vno, int mode, dev_t devno) {
  if(minix3_dentop(vno, name, OP_ADD, 0) == NULL)
    return -1;
  else
    return 0;
}

int minix3_link(struct vnode *vno, const char *name, struct vnode *new) {
  if(minix3_dentop(vno, name, OP_ADD, old->number) == NULL)
    return -1;
  else
    return 0;
}

int minix3_unlink(struct vnode *vno, const char *name) {
  if(minix3_dentop(vno, name, OP_UNLINK, 0) == NULL)
    return -1;
  else
    return 0;
}

int minix3_stat(struct vnode *vno, struct stat *buf) {
  struct minix3_vnode *m3vno = container_of(vno, struct minix3_vnode, vnode);
  struct minix3_fs *minix3 = container_of(vno->fs, struct minix3_fs, fs);

  bzero(buf, sizeof(struct stat));
  buf->st_dev = minix3->devno;
  buf->st_mode =  m3vno->minix3.i_mode;
  buf->st_size = m3vno->minix3.i_size;

  return 0;
}

