#include "fs.h"
#include "params.h"
#include "blkdev.h"
#include <stddef.h>
#include <stdint.h>


struct fsinfo *fsinfo_tbl[MAX_FSINFO];
static int fsinfotbl_used = 0;

struct fs *mount_tbl[MAX_MOUNT];
static int mounttbl_used = 0;

static struct inode *rootdir;


void fsinfo_add(struct fsinfo *info) {
  fsinfo_tbl[fsinfotbl_used++] = info;
}

int strcmp(const char *s1, const char *s2) {
  while(*s1 && *s1 == *s2) {
    s1++; s2++;
  }
  return *s1 - *s2;
}

int fs_mountroot(const char *name, void *source) {
  int i;
  for(i = 0; i<MAX_FSINFO; i++) {
    if(strcmp(fsinfo_tbl[i]->name, name) == 0) {
      break;
    }
  }

  if(i == MAX_FSINFO)
    return -1;

  struct fs *fs = fsinfo_tbl[i]->ops->mount(source);
  if(fs == NULL)
    return -1;
  mount_tbl[mounttbl_used++] = fs;
  rootdir = fs->ops->getroot(fs);
  if(rootdir == NULL) {
    mount_tbl[--mounttbl_used] = NULL;
    return -1;
  }
  return 0;
}


struct inode *fs_nametoi(const char *path) {
  const char *ptr = path;
  struct inode *curino = rootdir;
  if(curino == NULL) return NULL;
  // 現在は絶対パスのみ許可
  if(path == NULL) return NULL;
  if(*ptr != '/') return NULL;
  
  while(1) {
    while(*ptr == '/') ptr++;
    if(*ptr == '\0')
      return NULL;
    curino = curino->ops->opdent(curino, ptr, DENTOP_GET);
    while(*ptr && *ptr!='/') ptr++;
    if(*ptr == '\0')
      return curino;
  }
}

int fs_read(struct inode *inode, uint8_t *base, uint32_t offset, uint32_t count) {
  return inode->ops->read(inode, base, offset, count);
}
