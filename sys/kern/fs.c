#include <kern/fs.h>
#include <kern/kernlib.h>
#include <kern/blkdev.h>
#include <kern/file.h>

const struct fsinfo *fsinfo_tbl[MAX_FSINFO];
static int fsinfotbl_used = 0;

struct fs *mount_tbl[MAX_MOUNT];
static int mounttbl_used = 0;

static struct inode *rootdir;

FS_INIT void fs_init() {
  puts("Initializing filesystem...\n");
  fs_mountroot(ROOTFS_TYPE, ROOTFS_DEV);
}

void fsinfo_add(const struct fsinfo *info) {
  fsinfo_tbl[fsinfotbl_used++] = info;
}

int strcmp(const char *s1, const char *s2) {
  while(*s1 && *s1 == *s2) {
    s1++; s2++;
  }
  return *s1 - *s2;
}

static int fs_mountroot(const char *name, void *source) {
  int i;
  for(i = 0; i<fsinfotbl_used; i++) {
    if(strcmp(fsinfo_tbl[i]->name, name) == 0) {
      break;
    }
  }

  if(i == fsinfotbl_used)
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

static struct vnode *name_to_vnode(const char *path, struct vnode **parent) {
  const char *ptr = path;
  struct vnode *prevvno = NULL;
  struct vnode *curvno = rootdir;
  if(curvno == NULL) return NULL;
  // 現在は絶対パスのみ許可
  if(path == NULL) return NULL;
  if(*ptr != '/') return NULL;

  while(1) {
    while(*ptr == '/') ptr++;
    if(*ptr == '\0')
      return NULL;
    prevvno = curvno;
    curvno = curvno->ops->lookup(curvno, ptr);
    if(curvno == NULL)
      return NULL;
    while(*ptr && *ptr!='/') ptr++;
    if(*ptr == '\0') {
      if(parent != NULL)
        *parent = prevvno;
      return curvno;
    }
  }
}

struct file *open(const char *path) {
  int type;
  struct file *f;
  void *fcb;
  struct file_ops *ops;
  struct *vnode vno = name_to_vnode(path);
  if(vno == NULL)
    return NULL;
  f = file_new(vno->type, vno, vno->fs->file_ops);
  mutex_lock(&filelist_mtx);
  list_pushback(&f->link, &file_list);
  mutex_unlock(&filelist_mtx);
  return f;
}

int mknode(const char *path, int mode, dev_t dev) {
  struct *vnode vno = name_to_vnode(path);
  if(vno == NULL)
    return -1;
  return vno->ops->mknod(mode, dev);
}

int link(const char *oldpath, const char *newpath) {
  return -1;
}

int unlink(const char *path) {
  struct *vnode vno = name_to_vnode(path);
  if(vno == NULL)
    return -1;
  return vno->ops->unlink(mode, dev);
}

int stat(const char *path, struct stat *buf) {
  struct *vnode vno = name_to_vnode(path);
  if(vno == NULL)
    return -1;
  return vno->ops->stat(stat);
}

