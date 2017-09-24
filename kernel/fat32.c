#include "fat32.h"
#include "kernlib.h"
#include "fs.h"
#include "malloc.h"
#include "blkdev.h"
#include "common.h"
#include "vga.h"
#include <stdint.h>
#include <stddef.h>

#define FAT32_BOOT 0
#define FAT32_INODECACHE_SIZE 512

static struct fs *fat32_mount(void *source);
static struct inode *fat32_getroot(struct fs *fs);
static int fat32_inode_read(struct inode *inode, uint8_t *base, uint32_t offset, uint32_t count);
static struct inode *fat32_inode_opdent(struct inode *inode, const char *name, int op);


struct fsinfo_ops fat32_fsinfo_ops = {
  .mount = fat32_mount
};

struct fsinfo fat32_info = {
  .name = "fat32",
  .ops = &fat32_fsinfo_ops
};

struct fs_ops fat32_fs_ops = {
  .getroot = fat32_getroot
};


struct fat32_boot{
  uint8_t		BS_JmpBoot[3];
  uint8_t		BS_OEMName[8];
  uint16_t	BPB_BytsPerSec;
  uint8_t		BPB_SecPerClus;
  uint16_t	BPB_RsvdSecCnt;
  uint8_t		BPB_NumFATs;
  uint16_t	BPB_RootEntCnt;
  uint16_t	BPB_TotSec16;
  uint8_t		BPB_Media;
  uint16_t	BPB_FATSz16;
  uint16_t	BPB_SecPerTrk;
  uint16_t	BPB_NumHeads;
  uint32_t	BPB_HiddSec;
  uint32_t	BPB_TotSec32;
  uint32_t	BPB_FATSz32;
  uint16_t	BPB_ExtFlags;
  uint16_t	BPB_FSVer;
  uint32_t	BPB_RootClus;
  uint16_t	BPB_FSInfo;
  uint16_t	BPB_BkBootSec;
  uint8_t		BPB_Reserved[12];
  uint8_t		BS_DrvNum;
  uint8_t		BS_Reserved1;
  uint8_t		BS_BootSig;
  uint32_t	BS_VolID;
  uint8_t		BS_VolLab[11];
  uint8_t		BS_FilSysType[8];
  uint8_t		BS_BootCode32[420];
  uint16_t	BS_BootSign;
} PACKED;

struct fat32_fsi {
  uint32_t	FSI_LeadSig;
  uint8_t		FSI_Reserved1[480];
  uint32_t	FSI_StrucSig;
  uint32_t	FSI_Free_Count;
  uint32_t	FSI_Nxt_Free;
  uint8_t		FSI_Reserved2[12];
  uint32_t	FSI_TrailSig;
} PACKED;

#define ATTR_READ_ONLY	0x01
#define ATTR_HIDDEN			0x02
#define ATTR_SYSTEM			0x04
#define ATTR_VOLUME_ID	0x08
#define ATTR_DIRECTORY	0x10
#define ATTR_ARCHIVE		0x20
#define ATTR_LONG_NAME	(ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

struct fat32_dent {
  uint8_t 	DIR_Name[11];
  uint8_t		DIR_Attr;
  uint8_t		DIR_NTRes;
  uint8_t		DIR_CrtTimeTenth;
  uint16_t	DIR_CrtTime;
  uint16_t	DIR_CrtDate;
  uint16_t	DIR_LstAccDate;
  uint16_t	DIR_FstClusHI;
  uint16_t	DIR_WrtTime;
  uint16_t	DIR_WrtDate;
  uint16_t	DIR_FstClusLO;
  uint32_t	DIR_FileSize;
} PACKED;

struct fat32_lfnent {
  uint8_t		LDIR_Ord;
  uint8_t		LDIR_Name1[10];
  uint8_t		LDIR_Attr;
  uint8_t		LDIR_Type;
  uint8_t		LDIR_Chksum;
  uint8_t		LDIR_Name2[12];
  uint16_t	LDIR_FstClusLO;
  uint8_t		LDIR_Name3[4];
} PACKED;

struct fat32_fs {
  uint16_t devno;
  struct fat32_boot boot;
  struct fat32_fsi fsi;
  struct fs fs;
  uint32_t fatstart;
  uint32_t fatsectors;
  uint32_t rootstart;
  uint32_t rootsectors;
  uint32_t datastart;
  uint32_t datasectors;
};

struct inode_ops fat32_inode_ops = {
  .read = fat32_inode_read,
  .write = NULL,
  .resize = NULL,
  .opdent = fat32_inode_opdent
};

struct fat32_inode {
  uint32_t cluster;
  struct inode inode;
};


void fat32_init() {
  fsinfo_add(&fat32_info);
}

static int fat32_is_valid_boot(struct fat32_boot *boot) {
  if(boot->BS_BootSign != 0xaa55)
    return 0;
  if(boot->BPB_BytsPerSec != BLOCKSIZE)
    return 0;
  return 1;
}

static struct fs *fat32_mount(void *source) {
  uint16_t devno = (uint16_t)source;
  struct fat32_fs *fat32 = malloc(sizeof(struct fat32_fs));
  fat32->devno = devno;
  struct blkdev_buf *buf = blkdev_getbuf(devno, FAT32_BOOT);
  blkdev_buf_sync(buf);
  fat32->boot = *(struct fat32_boot *)(buf->addr);
  struct fat32_boot *boot = &(fat32->boot);
  blkdev_releasebuf(buf);
  fat32->fs.ops = &fat32_fs_ops;

  fat32->fatstart = boot->BPB_RsvdSecCnt;
  fat32->fatsectors = boot->BPB_FATSz32 * boot->BPB_NumFATs;
  fat32->rootstart = fat32->fatstart + fat32->fatsectors;
  fat32->rootsectors = (sizeof(struct fat32_dent) * boot->BPB_RootEntCnt + boot->BPB_BytsPerSec - 1) / boot->BPB_BytsPerSec;
  fat32->datastart = fat32->rootstart + fat32->rootsectors;
  fat32->datasectors = boot->BPB_TotSec32 - fat32->datastart;
  if(fat32->datasectors/boot->BPB_SecPerClus < 65526
      || !fat32_is_valid_boot(boot)) {
    free(fat32);
    puts("not a fat32 filesystem.");
    return NULL;
  }
  return &(fat32->fs);
}

static struct inode *fat32_getroot(struct fs *fs) {
  struct fat32_fs *fat32fs = container_of(fs, struct fat32_fs, fs);
  struct fat32_inode *ino = malloc(sizeof(struct fat32_inode));
  ino->cluster = fat32fs->boot.BPB_RootClus;
  ino->inode.fs = fs;
  ino->inode.ops = &(fat32_inode_ops);
  ino->inode.inode_no = ino->cluster;
  ino->inode.mode = INODE_DIR;
  ino->inode.size = 0;
  return &(ino->inode);
}

static uint32_t fat32_fat_at(struct fat32_fs *f, uint32_t index) {
  struct fat32_boot *boot = &(f->boot);
  uint32_t sector = f->fatstart + (index*4/boot->BPB_BytsPerSec);
  uint32_t offset = (index*4)%boot->BPB_BytsPerSec;
  struct blkdev_buf *buf = blkdev_getbuf(f->devno, sector);
  blkdev_buf_sync(buf);
  uint32_t entry = buf->addr[offset] & 0x0fffffff;
  blkdev_releasebuf(buf);
  return entry;
}

static int fat32_inode_read(struct inode *inode, uint8_t *base, uint32_t offset, uint32_t count) {
/*
  uint32_t tail = count + offset;
  tail = (tail > inode->size) ? inode->size : tail;

  struct fat32_fs *f = container_of(inode->fs, struct fat32_fs, fs);
  uint16_t devno = f->devno;
  struct fat32_inode *v6ino = container_of(inode, struct fat32_inode, inode);
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
*/
  return 0;
}

static int strcmp_dent(const char *path, const char *name) {
  while(*path && *path!='/' && *path == *name) {
    path++; name++;
  }
  if(*path == '/' && *name == '\0')
    return 0;
  return *path - *name;
}

static uint32_t cluster_to_sector(struct fat32_fs *f, uint32_t cluster) {
  return f->datastart + (cluster-2) * f->boot.BPB_SecPerClus;
}

static uint8_t create_sum(struct fat32_dent *entry) {
  int i;
  uint8_t sum;

  for (i = sum = 0; i < 11; i++)
    sum = (sum >> 1) + (sum << 7) + entry->DIR_Name[i];
  return sum;
}

static char *get_lfn(struct fat32_dent *sfnent) {
  struct fat32_lfnent *lfnent = (struct fat32_lfnent *)sfnent - 1;
  uint8_t sum = create_sum(sfnent);
  static char name[256];
  char *ptr = name;
  int seq = 1;
  while(1) {
    if(lfnent->LDIR_Attr!=ATTR_LONG_NAME
          || lfnent->LDIR_Chksum!=sum
          || lfnent->LDIR_Ord!=seq++) {
      return NULL;
    }
    memcpy(ptr, lfnent->LDIR_Name1, 10);
    memcpy(ptr, lfnent->LDIR_Name2, 12);
    memcpy(ptr, lfnent->LDIR_Name3, 4);
    if(lfnent->LDIR_Ord & 0x40)
      break;
    lfnent--;
  }
  return name;
}

static struct inode *fat32_inode_opdent(struct inode *inode, const char *name, int op) {
  struct fat32_fs *f = container_of(inode->fs, struct fat32_fs, fs);
  devno_t devno = f->devno;
  struct fat32_inode *fatino = container_of(inode, struct fat32_inode, inode);
  struct blkdev_buf *buf = NULL;
  uint32_t current_cluster = fatino->cluster;
  int found = 0;
  struct fat32_dent found_dent;
  if((inode->mode & INODE_DIR) == 0)
    goto exit;

  for(uint32_t i=0; ; i+=sizeof(struct fat32_dent)) {
    if(i%BLOCKSIZE == 0) {
      if(buf != NULL)
        blkdev_releasebuf(buf);
      if(current_cluster == 0)
        break;
      if(current_cluster >= 0x0ffffff8
           && current_cluster <= 0x0fffffff)
        break;
      buf = blkdev_getbuf(devno, cluster_to_sector(f, current_cluster) + i/BLOCKSIZE);
      blkdev_buf_sync(buf);
      if(i/BLOCKSIZE == f->boot.BPB_SecPerClus-1)
        current_cluster = fat32_fat_at(f, current_cluster);
    }

    struct fat32_dent *dent = (struct fat32_dent*)((uint8_t *)(buf->addr)+(i%512));
    if(dent->DIR_Attr & ATTR_VOLUME_ID)
      continue;
    if(dent->DIR_Attr & ATTR_LONG_NAME)
      continue;
    switch(op) {
    case DENTOP_GET:
      if(strcmp_dent(name, get_lfn(dent)) == 0) {
        found = 1;
        found_dent = *dent;
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
    else {
      struct fat32_inode *ino = malloc(sizeof(struct fat32_inode));
      ino->cluster = (found_dent.DIR_FstClusHI<<16) | found_dent.DIR_FstClusLO;
      ino->inode.fs = &(f->fs);
      ino->inode.ops = &(fat32_inode_ops);
      ino->inode.inode_no = ino->cluster;
      ino->inode.mode = (found_dent.DIR_Attr&ATTR_DIRECTORY)?INODE_DIR:0;
      ino->inode.size = found_dent.DIR_FileSize;
      return &(ino->inode);
    }
    break;
  }

  return NULL;
}

