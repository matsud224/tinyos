#include <kern/kernlib.h>
#include <kern/fs.h>
#include <kern/file.h>
#include <kern/blkdev.h>

#define FAT32_BOOT 0
#define FAT32_INODECACHE_SIZE 512

static struct fs *fat32_mount(devno_t devno);
static struct vnode *fat32_getroot(struct fs *fs);

static const struct fstype_ops fat32_fstype_ops = {
  .mount = fat32_mount,
};

static const struct fs_ops fat32_fs_ops = {
  .getroot = fat32_getroot,
};


struct fat32_boot{
  u8	BS_JmpBoot[3];
  u8	BS_OEMName[8];
  u16	BPB_BytsPerSec;
  u8	BPB_SecPerClus;
  u16	BPB_RsvdSecCnt;
  u8	BPB_NumFATs;
  u16	BPB_RootEntCnt;
  u16	BPB_TotSec16;
  u8	BPB_Media;
  u16	BPB_FATSz16;
  u16	BPB_SecPerTrk;
  u16	BPB_NumHeads;
  u32	BPB_HiddSec;
  u32	BPB_TotSec32;
  u32	BPB_FATSz32;
  u16	BPB_ExtFlags;
  u16	BPB_FSVer;
  u32	BPB_RootClus;
  u16	BPB_FSInfo;
  u16	BPB_BkBootSec;
  u8	BPB_Reserved[12];
  u8	BS_DrvNum;
  u8	BS_Reserved1;
  u8	BS_BootSig;
  u32	BS_VolID;
  u8	BS_VolLab[11];
  u8	BS_FilSysType[8];
  u8	BS_BootCode32[420];
  u16	BS_BootSign;
} PACKED;

struct fat32_fsi {
  u32	FSI_LeadSig;
  u8	FSI_Reserved1[480];
  u32	FSI_StrucSig;
  u32	FSI_Free_Count;
  u32	FSI_Nxt_Free;
  u8	FSI_Reserved2[12];
  u32	FSI_TrailSig;
} PACKED;

#define ATTR_READ_ONLY	0x01
#define ATTR_HIDDEN			0x02
#define ATTR_SYSTEM			0x04
#define ATTR_VOLUME_ID	0x08
#define ATTR_DIRECTORY	0x10
#define ATTR_ARCHIVE		0x20
#define ATTR_LONG_NAME	(ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

struct fat32_dent {
  u8 	DIR_Name[11];
  u8	DIR_Attr;
  u8	DIR_NTRes;
  u8	DIR_CrtTimeTenth;
  u16	DIR_CrtTime;
  u16	DIR_CrtDate;
  u16	DIR_LstAccDate;
  u16	DIR_FstClusHI;
  u16	DIR_WrtTime;
  u16	DIR_WrtDate;
  u16	DIR_FstClusLO;
  u32	DIR_FileSize;
} PACKED;

struct fat32_lfnent {
  u8	LDIR_Ord;
  u8	LDIR_Name1[10];
  u8	LDIR_Attr;
  u8	LDIR_Type;
  u8	LDIR_Chksum;
  u8	LDIR_Name2[12];
  u16	LDIR_FstClusLO;
  u8	LDIR_Name3[4];
} PACKED;

struct fat32_fs {
  u16 devno;
  struct fat32_boot boot;
  struct fat32_fsi fsi;
  struct fs fs;
  u32 fatstart;
  u32 fatsectors;
  u32 rootstart;
  u32 rootsectors;
  u32 datastart;
  u32 datasectors;
};

enum fat32_vnode_type {
  FAT32_V_FILE,
  FAT32_V_DIR,
};

struct fat32_vnode {
  int type;
  u32	size;
  u32 cluster;
  struct vnode vnode;
};

#define is_active_cluster(c)  ((c) >= 0x2 && (c) <= 0x0ffffff6)
#define is_terminal_cluster(c) ((c) >= 0x0ffffff8 && (c) <= 0x0fffffff)
#define dir_cluster(dir) (((dir)->DIR_FstClusHI << 16) | (dir)->DIR_FstClusLO)
#define UNUSED_CLUSTER 0
#define RESERVED_CLUSTER 1
#define BAD_CLUSTER 	0x0FFFFFF7

int fat32_read(struct file *f, void *buf, size_t count);
int fat32_lseek(struct file *f, off_t offset, int whence);

static const struct file_ops fat32_file_ops = {
  .read = fat32_read,
  .lseek = fat32_lseek, 
};

struct vnode *fat32_lookup(struct vnode *vno, const char *name);
int fat32_stat(struct vnode *vno, struct stat *buf);

static const struct vnode_ops fat32_vnode_ops = {
  .lookup = fat32_lookup,
  .stat = fat32_stat,
};


FS_INIT void fat32_init() {
  fstype_register("fat32", &fat32_fstype_ops);
}

static int fat32_is_valid_boot(struct fat32_boot *boot) {
  if(boot->BS_BootSign != 0xaa55)
    return 0;
  if(boot->BPB_BytsPerSec != BLOCKSIZE)
    return 0;
  return 1;
}

static struct fs *fat32_mount(devno_t devno) {
  struct fat32_fs *fat32 = malloc(sizeof(struct fat32_fs));
  fat32->devno = devno;
  struct blkbuf *buf = blkbuf_get(devno, FAT32_BOOT);
  blkbuf_sync(buf);
  fat32->boot = *(struct fat32_boot *)(buf->addr);
  struct fat32_boot *boot = &(fat32->boot);
  blkbuf_release(buf);

  fat32->fatstart = boot->BPB_RsvdSecCnt;
  fat32->fatsectors = boot->BPB_FATSz32 * boot->BPB_NumFATs;
  fat32->rootstart = fat32->fatstart + fat32->fatsectors;
  fat32->rootsectors = (sizeof(struct fat32_dent) * boot->BPB_RootEntCnt + boot->BPB_BytsPerSec - 1) / boot->BPB_BytsPerSec;
  fat32->datastart = fat32->rootstart + fat32->rootsectors;
  fat32->datasectors = boot->BPB_TotSec32 - fat32->datastart;
  if(fat32->datasectors/boot->BPB_SecPerClus < 65526
      || !fat32_is_valid_boot(boot)) {
    free(fat32);
    puts("Bad fat32 filesystem.");
    return NULL;
  }
  return &(fat32->fs);
}

static struct vnode *fat32_getroot(struct fs *fs) {
  struct fat32_fs *fat32fs = container_of(fs, struct fat32_fs, fs);
  struct fat32_vnode *vno = malloc(sizeof(struct fat32_vnode));
  vno->cluster = fat32fs->boot.BPB_RootClus;
  vno->vnode.fs = fs;
  vno->vnode.ops = &(fat32_vnode_ops);
  vno->vnode.number = vno->cluster;
  vno->vnode.type = V_REGULAR;
  return &(vno->vnode);
}

static u32 fatent_read(struct fat32_fs *f, u32 index) {
  struct fat32_boot *boot = &(f->boot);
  u32 sector = f->fatstart + (index*4/boot->BPB_BytsPerSec);
  u32 offset = index * 4 % boot->BPB_BytsPerSec;
  struct blkbuf *buf = blkbuf_get(f->devno, sector);
  blkbuf_sync(buf);
  u32 entry = *((u32*)((u8*)(buf->addr) + offset)) & 0x0fffffff;
  blkbuf_release(buf);
  return entry;
}

static u32 fatent_write(struct fat32_fs *f, u32 index, u32 value) {
  struct fat32_boot *boot = &(f->boot);
  u32 sector = f->fatstart + (index*4/boot->BPB_BytsPerSec);
  u32 offset = index * 4 % boot->BPB_BytsPerSec;
  struct blkbuf *buf = blkbuf_get(f->devno, sector);
  blkbuf_sync(buf);
  u32 entry = *((u32*)((u8*)(buf->addr) + offset));
  entry = (entry & 0xf0000000) | (value & 0x0fffffff);
  *((u32*)((u8*)(buf->addr) + offset)) = entry;
  blkbuf_release(buf);
  return entry;
}

static u32 walk_cluster_chain(struct fat32_fs *f, u32 offset, u32 cluster) {
  int nlook = offset / (f->boot.BPB_SecPerClus * f->boot.BPB_BytsPerSec);
  for(int i=0; i<nlook; i++) {
    cluster = fatent_read(f, cluster);
    if(!is_active_cluster(cluster))
      return BAD_CLUSTER;
  }
  return cluster;
}

static void release_cluster_chain(struct fat32_fs *f, u32 cluster) {
  while(cluster != BAD_CLUSTER) {
    u32 tmp = fatent_read(f, cluster);
    fatent_write(f, cluster, UNUSED_CLUSTER);
    cluster = tmp;
  }
}

static void show_cluster_chain(struct fat32_fs *f, u32 cluster) {
  while(1) {
    printf("\tchain: %d\n", cluster);
    cluster = fatent_read(f, cluster);
    if(!is_active_cluster(cluster))
      return;
  }
}

static u32 cluster_to_sector(struct fat32_fs *f, u32 cluster);

static int strcmp_dent(const char *path, const char *name) {
  while(*path && *path!='/' && *path == *name) {
    path++; name++;
  }
  if(*path == '/' && *name == '\0')
    return 0;
  return *path - *name;
}

static u32 cluster_to_sector(struct fat32_fs *f, u32 cluster) {
  return f->datastart + (cluster-2) * f->boot.BPB_SecPerClus;
}

static u8 create_sum(struct fat32_dent *entry) {
  int i;
  u8 sum;

  for (i = sum = 0; i < 11; i++)
    sum = (sum >> 1) + (sum << 7) + entry->DIR_Name[i];
  return sum;
}

static char *get_sfn(struct fat32_dent *sfnent) {
  static char name[13];
  char *ptr = name;
  for(int i=0; i<=7; i++, ptr++) {
    *ptr = sfnent->DIR_Name[i];
    if(*ptr == 0x05)
      *ptr = 0xe5;
    if(*ptr == ' ')
      break;
  }
  if(sfnent->DIR_Name[8] != ' ') {
    *ptr++ = '.';
    for(int i=8; i<=10; i++, ptr++) {
      *ptr = sfnent->DIR_Name[i];
      if(*ptr == 0x05)
        *ptr = 0xe5;
      if(*ptr == ' ')
        break;
    }
  }
  *ptr = '\0';
  return name;
}
 
static char *get_lfn(struct fat32_dent *sfnent) {
  struct fat32_lfnent *lfnent = (struct fat32_lfnent *)sfnent - 1;
  u8 sum = create_sum(sfnent);
  static char name[256];
  char *ptr = name;
  int seq = 1;

  while(1) {
    if((lfnent->LDIR_Attr&ATTR_LONG_NAME)!=ATTR_LONG_NAME
          || lfnent->LDIR_Chksum!=sum
          || (lfnent->LDIR_Ord&0x1f)!=seq++) {
      return NULL;
    }
    for(int i=0; i<10; i+=2)
      *ptr++ = lfnent->LDIR_Name1[i]; //UTF16-LE
    for(int i=0; i<12; i+=2)
      *ptr++ = lfnent->LDIR_Name2[i]; //UTF16-LE
    for(int i=0; i<4; i+=2)
      *ptr++ = lfnent->LDIR_Name3[i]; //UTF16-LE
    if(lfnent->LDIR_Ord & 0x40)
      break;
    lfnent--;
  }
  return name;
}

struct vnode *fat32_lookup(struct vnode *vno, const char *name) {
  struct fat32_fs *f = container_of(vno->fs, struct fat32_fs, fs);
  devno_t devno = f->devno;
  struct fat32_vnode *fatvno = container_of(vno, struct fat32_vnode, vnode);
  struct blkbuf *buf = NULL;

  struct fat32_dent *found_dent = NULL;

  u32 current_cluster = fatvno->cluster;
  u32 secs_per_clus = f->boot.BPB_SecPerClus;
  
  while(1) {
    if(!is_active_cluster(current_cluster))
      break;

    for(u32 sec = 0; sec < secs_per_clus; sec++) {
      if(buf != NULL)
        blkbuf_release(buf);
      buf = blkbuf_get(devno, cluster_to_sector(f, current_cluster) + sec);
      blkbuf_sync(buf);

      for(u32 i=0; i<BLOCKSIZE; i+=sizeof(struct fat32_dent)) {
        struct fat32_dent *dent = (struct fat32_dent*)(buf->addr + i);
        if(dent->DIR_Name[0] == 0x00)
          break;
        if(dent->DIR_Name[0] == 0xe5)
          continue;
        if(dent->DIR_Attr & (ATTR_VOLUME_ID|ATTR_LONG_NAME))
          continue;

        char *dent_name = NULL;
        if(i%BLOCKSIZE != 0)
          dent_name = get_lfn(dent);
        if(dent_name == NULL)
          dent_name = get_sfn(dent);

        if(strcmp_dent(name, dent_name) == 0) {
          found_dent = dent;
          goto exit1;
        }
      }
    }

    current_cluster = fatent_read(f, current_cluster);
  }
  
exit1:
  if(found_dent == NULL)
    return NULL;

  struct fat32_vnode *newvno = malloc(sizeof(struct fat32_vnode));
  newvno->cluster = (found_dent->DIR_FstClusHI<<16) | found_dent->DIR_FstClusLO;
  if(fatvno->cluster == 0) {
    // root directory
    newvno->cluster = f->boot.BPB_RootClus;
  }
  newvno->vnode.fs = &(f->fs);
  newvno->vnode.ops = &fat32_vnode_ops;
  return &(newvno->vnode);
}


int fat32_read(struct file *f, void *buf, size_t count) {
  struct vnode *vno = (struct vnode *)f->data;
  struct fat32_vnode *fatvno = container_of(vno, struct fat32_vnode, vnode);
  u32 offset = f->offset;
  u32 tail = MIN(count + offset, fatvno->size);
  u32 remain = tail - offset;

  struct fat32_fs *ffs = container_of(vno->fs, struct fat32_fs, fs);
  devno_t devno = ffs->devno;
  struct blkbuf *bbuf = NULL;

  if(fatvno->size <= offset)
    return 0;

  u32 current_cluster = walk_cluster_chain(ffs, offset, fatvno->cluster);
  u32 secs_per_clus = ffs->boot.BPB_SecPerClus;
  u32 in_clus_off = offset % (ffs->boot.BPB_SecPerClus * ffs->boot.BPB_BytsPerSec);

  while(remain > 0) {
    if(!is_active_cluster(current_cluster))
      break;

    u32 in_blk_off = in_clus_off % BLOCKSIZE;
    for(u32 sec = in_clus_off / BLOCKSIZE; remain > 0 && sec < secs_per_clus; sec++) {
      if(buf != NULL)
        blkbuf_release(bbuf);

      bbuf = blkbuf_get(devno, cluster_to_sector(ffs, current_cluster) + sec);
      blkbuf_sync(bbuf);
      u32 copylen = MIN(BLOCKSIZE - in_blk_off, remain);
      memcpy(buf, bbuf->addr + in_blk_off, copylen);
      buf += copylen;
      remain -= copylen;
      in_blk_off = 0;
    }

    current_cluster = fatent_read(ffs, current_cluster);
    in_clus_off = 0;
  }
  
  if(bbuf != NULL)
    blkbuf_release(bbuf);

  u32 read_bytes = (tail - offset) - remain;
  f->offset += read_bytes;
  return read_bytes;
}

int fat32_lseek(struct file *f, off_t offset, int whence) {
  struct vnode *vno = (struct vnode *)f->data;
  struct fat32_vnode *fatvno = container_of(vno, struct fat32_vnode, vnode);
  switch(whence) {
  case SEEK_SET:
    f->offset = offset;
    break;
  case SEEK_CUR:
    f->offset += offset;
    break;
  case SEEK_END:
    f->offset = fatvno->size;
    break;
  default:
    return -1;
  }
 
  return 0;
}

int fat32_stat(struct vnode *vno, struct stat *buf) {
  struct fat32_vnode *fatvno = container_of(vno, struct fat32_vnode, vnode);
  struct fat32_fs *f = container_of(vno->fs, struct fat32_fs, fs);

  bzero(buf, sizeof(struct stat));
  buf->st_dev = f->devno;
  buf->st_size = fatvno->size;

  return 0;
}

