#include <kern/fs.h>
#include <kern/kernlib.h>
#include <kern/blkdev.h>
#include <kern/chardev.h>
#include <kern/file.h>

struct fstype {
  const char *name;
  const struct fstype_ops *ops;
};

static struct fstype fstype_tbl[MAX_FSTYPE];
static int fstypetbl_used = 0;

struct fs *mount_tbl[MAX_MOUNT];
static int mounttbl_used = 0;

static struct vnode *rootdir;


static int fs_mountroot(const char *name, devno_t devno);

FS_INIT void fs_init() {
  puts("Initializing filesystem...\n");
  fs_mountroot(ROOTFS_TYPE, ROOTFS_DEV);
}

void fstype_register(const char *name, const struct fstype_ops *ops) {
  fstype_tbl[fstypetbl_used].name = name;
  fstype_tbl[fstypetbl_used].ops = ops;
  fstypetbl_used++;
  printf("Filesystem \"%s\" registered.\n", name);
}

int strcmp(const char *s1, const char *s2) {
  while(*s1 && *s1 == *s2) {
    s1++; s2++;
  }
  return *s1 - *s2;
}

static int fs_mountroot(const char *name, devno_t devno) {
  int i;
  for(i = 0; i<fstypetbl_used; i++)
    if(strcmp(fstype_tbl[i].name, name) == 0)
      break;

  if(i == fstypetbl_used)
    return -1;

  struct fs *fs = fstype_tbl[i].ops->mount(devno);
  if(fs == NULL)
    return -1;
  mount_tbl[mounttbl_used++] = fs;
  rootdir = fs->fs_ops->getroot(fs);
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
    if(curvno->ops->lookup)
      curvno = curvno->ops->lookup(curvno, ptr);
    else
      curvno = NULL;

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
  struct file *f;
  struct vnode *vno = name_to_vnode(path, NULL);
  if(vno == NULL)
    return NULL;
  switch(vno->type) {
  case V_REGULAR:
    f = file_new(vno, vno->fs->file_ops);
    break;
  case V_BLKDEV:
    f = file_new(vno, &blkdev_file_ops);
    break;
  case V_CHARDEV:
    f = file_new(vno, &chardev_file_ops);
    break;
  default:
    //vnode_release(vno);
    return NULL;
  } 
  return f;
}

int mknod(const char *path, int mode, devno_t devno) {
  struct vnode *vno = name_to_vnode(path, NULL);
  if(vno == NULL)
    return -1;
  if(!vno->ops->mknod)
    return -1;

  return vno->ops->mknod(vno, mode, devno);
}

int link(const char *oldpath, const char *newpath) {
  struct vnode *vno0 = name_to_vnode(oldpath, NULL);
  if(vno0 == NULL)
    return -1;
  if(!vno0->ops->link)
    return -1;

  
}

int unlink(const char *path) {
  struct vnode *vno = name_to_vnode(path, NULL);
  if(vno == NULL)
    return -1;
  if(!vno->ops->unlink)
    return -1;

  return vno->ops->unlink(vno, path);
}

int stat(const char *path, struct stat *buf) {
  struct vnode *vno = name_to_vnode(path, NULL);
  if(vno == NULL)
    return -1;
  if(!vno->ops->stat)
    return -1;

  return vno->ops->stat(vno, buf);
}

