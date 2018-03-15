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

static struct fstype fstype_tbl[MAX_FSTYPE];
static struct vnode *vcache[NVNODES];
static mutex vcache_mtx;

static struct vnode *rootdir;


static int fs_mountroot(const char *name, devno_t devno);

FS_INIT void fs_init() {
  puts("Initializing filesystem...\n");
  
  mutex_init(&vcache_mtx);  

  for(int i=0; i<NVODES; i++) {
    vcache[i] = NULL;
  }

  fs_mountroot(ROOTFS_TYPE, ROOTFS_DEV);
}

void fstype_register(const char *name, const struct fstype_ops *ops) {
  fstype_tbl[fstypetbl_used].name = name;
  fstype_tbl[fstypetbl_used].ops = ops;
  fstypetbl_used++;
  printf("Filesystem \"%s\" registered.\n", name);
}

void vnode_init(struct vnode *vno) {
  bzero(vno, sizeof(struct vnode));
  mutex_init(&vno->mtx);
  vno->ref = 1;
}

void vnode_lock(struct vnode *vno) {
  mutex_lock(&vno->mtx);
}

void vnode_unlock(struct vnode *vno) {
  mutex_unlock(&vno->mtx);
}

void vnode_hold(struct vnode *vno) {
  vnode_lock(vno);
  if(vno->ref > 0)
    vno->ref++;
  vnode_unlock(vno);
}

void vnode_release(struct vnode *vno) {
  vnode_lock(vno);
  if(vno->ref > 0)
    vno->ref--;
  vnode_unlock(vno);
}

struct vnode *vcache_find(struct fs *fs, vno_t number) {
  mutex_lock(&vcache_mtx);
  struct list_head *p;
  list_head(p, &fs->vnode_list) {
    struct vnode *vno = list_entry(p, struct vnode, fs_link);
    if(vno->number == number) {
      vnode_hold(vno);
      mutex_unlock(&vcache_mtx);
      return vno;
    } 
  }
  mutex_unlock(&vcache_mtx);
}

int vcache_add(struct fs *fs, struct vnode *vno) {
  mutex_lock(&vcache_mtx);
  vno_t i, can_free = VNO_INVALID;
  for(i=0; i<NVNODES; i++) {
    if(vcache[i] == NULL)
      break;
    else if(vcache[i]->ref == 0)
      can_free = i;
  }
  if(i != NVNODES) {
    vcache[i] = vno;
    list_pushfront(&vno->fs_link, &fs->vnode_list);
    mutex_unlock(&vcache_mtx);
    return 0;
  } else if(can_free != VNO_INVALID) {
    if(vcache[can_free]->ops.vfree)
      vcache[can_free]->ops.vfree(vcache[can_free]);
    else
      free(vcache[can_free]);

    vcache[can_free] = vno;
    list_pushfront(&vno->fs_link, &fs->vnode_list);
    mutex_unlock(&vcache_mtx);
    return 0;
  }

  mutex_unlock(&vcache_mtx);
  return -1;
}

void vcache_remove(struct vnode *vno) {
  mutex_lock(&vcache_mtx);
  for(i=0; i<NVNODES; i++) {
    if(vcache[i] == vno) {
      vcache[i] = NULL;
      list_remove(&vno->fs_link);
      mutex_unlock(&vcache_mtx);
      return; 
    }
  }
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
  list_init(&fs->vnode_list);
  mount_tbl[mounttbl_used++] = fs;
  rootdir = fs->fs_ops->getroot(fs);
  if(rootdir == NULL) {
    mount_tbl[--mounttbl_used] = NULL;
    return -1;
  }
  return 0;
}

static struct vnode *name_to_vnode(const char *path, struct vnode **parent, char **fname) {
  static char name[MAX_FILE_NAME+1];
  const char *ptr = path;
  struct vnode *prevvno = NULL;
  struct vnode *curvno = rootdir;

  if(rootdir == NULL) return NULL;
  vnode_hold(rootdir);
  vnode_lock(rootdir);

  if(path == NULL) return NULL;
  // 現在は絶対パスのみ許可
  if(*ptr != '/') return NULL;

  while(1) {
    while(*ptr == '/') ptr++;

    if(*ptr == '\0') {
      if(parent != NULL)
        *parent = prevvno;
      else if(prevvno != NULL) {
        vnode_unlock(prevvno);
        vnode_release(prevvno);
      }
      if(fname != NULL)
        *fname = strdup(name);
      return curvno;
    }

    int i;
    for(i=0; *ptr != '\0' && *ptr != '/' && i<MAX_FILE_NAME+1; i++, ptr++) {
      name[i] = *ptr;
    }
    if(i == MAX_FILE_NAME+1)
      break;
    else
      name[i] = NULL;

    if(prevvno != NULL) {
      vnode_unlock(prevvno);
      vnode_release(prevvno);
    }

    prevvno = curvno;
    if(curvno->ops->lookup) {
      vnode_lock(curvno);
      curvno = curvno->ops->lookup(curvno, name);
    } else {
      break;
    }
  }

  if(curvno != NULL) {
    vnode_unlock(curvno);
    vnode_release(curvno);
  }
  
  if(prevvno != NULL) {
    vnode_unlock(prevvno);
    vnode_release(prevvno);
  }

  return NULL;
}

struct file *open(const char *path) {
  struct file *f;
  struct vnode *parent = NULL;
  char *name = NULL;
  struct vnode *vno = name_to_vnode(path, &parent, &name);
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
  struct vnode *parent = NULL;
  char *name = NULL;
  struct vnode *vno = name_to_vnode(path, &parent, &name);
  int ret = -1;
  if(vno != NULL)
    goto err;
  if(!parent->ops->mknod)
    goto err;

  ret = parent->ops->mknod(parent, name, mode, devno);

err:
  if(vno != NULL) {
    vnode_unlock(vno);
    vnode_release(vno);
  }
  if(parent != NULL) {
    vnode_unlock(parent);
    vnode_release(parent);
  }
  return ret;
}

int link(const char *oldpath, const char *newpath) {
  struct vnode *vno0 = NULL, *vno1 = NULL;
  struct vnode *parent = NULL;
  char *name = NULL;
  int ret = -1;

  vno0 = name_to_vnode(oldpath, NULL, NULL);
  if(vno0 == NULL)
    goto err;

  vno1 = name_to_vnode(newpath, &parent, &name);
  if(vno1 != NULL)
    goto err;

  if(!parent->ops->create)
    goto err;

  if(parent->fs != vno0->fs)
    goto err;

  if(!parent->ops->link)
    goto err;

  ret = parent->ops->link(parent, name, vno0);

err:
  if(vno0 != NULL) {
    vnode_unlock(vno0);
    vnode_release(vno0);
  }
  if(vno1 != NULL) {
    vnode_unlock(vno1);
    vnode_release(vno1);
  }
  if(parent != NULL) {
    vnode_unlock(parent);
    vnode_release(parent);
  }
  if(name != NULL)
    free(name);
  return ret;
}

int unlink(const char *path) {
  struct vnode *parent = NULL;
  char *name = NULL;
  struct vnode *vno = name_to_vnode(path, &parent, &name);
  int ret = -1;
  if(vno == NULL)
    goto err;
  if(!vno->ops->unlink)
    goto err;

  vnode_unlock(vno);
  vnode_release(vno);
  ret = parent->ops->unlink(parent, name);

err:
  if(vno != NULL) {
    vnode_unlock(vno);
    vnode_release(vno);
  }
  if(parent != NULL) {
    vnode_unlock(parent);
    vnode_release(parent);
  }
  if(name != NULL)
    free(name);
  return ret;
}

int stat(const char *path, struct stat *buf) {
  struct vnode *vno = name_to_vnode(path, NULL);
  int ret = -1;
  if(vno == NULL)
    goto err;
  if(!vno->ops->stat)
    goto err;

  ret = vno->ops->stat(vno, buf);

err:
  if(vno != NULL) {
    vnode_unlock(vno);
    vnode_release(vno);
  }
  return ret 
}
