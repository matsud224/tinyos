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
static struct vnode *vcache[NVCACHE];
static mutex vcache_mtx;

static struct vnode *rootdir;

void fs_init() {
  mutex_init(&vcache_mtx);  

  for(int i=0; i<NVCACHE; i++) {
    vcache[i] = NULL;
  }
}

void fstype_register(const char *name, const struct fstype_ops *ops) {
  fstype_tbl[fstypetbl_used].name = name;
  fstype_tbl[fstypetbl_used].ops = ops;
  fstypetbl_used++;
  printf("fs: \"%s\" registered\n", name);
}

void vnode_init(struct vnode *vno, vno_t number, struct fs *fs, const struct vnode_ops *ops, const struct file_ops *file_ops) {
  bzero(vno, sizeof(struct vnode));
  mutex_init(&vno->mtx);
  vno->ref = 1;
  vno->number = number;
  vno->fs = fs;
  vno->ops = ops;
  vno->file_ops = file_ops;
}

void vnode_lock(struct vnode *vno) {
  mutex_lock(&vno->mtx);
}

void vnode_unlock(struct vnode *vno) {
  if(vno == NULL)
    return;
  mutex_unlock(&vno->mtx);
}

void vnode_hold(struct vnode *vno) {
  vnode_lock(vno);
  if(vno->ref > 0)
    vno->ref++;
  vnode_unlock(vno);
}

void vnode_release(struct vnode *vno) {
  if(vno == NULL)
    return;
  vnode_lock(vno);
  if(vno->ref > 0)
    vno->ref--;
  vnode_unlock(vno);
}

void vnode_markdirty(struct vnode *vno) {
  if(vno == NULL)
    return;
  vno->flags |= V_DIRTY;
}

void vnode_sync(struct vnode *vno) {
  if(vno == NULL)
    return;
  vnode_lock(vno);
  if(vno->ops->vsync)
    vno->ops->vsync(vno); 
  vnode_unlock(vno);
}

struct vnode *vcache_find(struct fs *fs, vno_t number) {
  mutex_lock(&vcache_mtx);
  struct list_head *p;
  list_foreach(p, &fs->vnode_list) {
    struct vnode *vno = list_entry(p, struct vnode, fs_link);
    if(vno->number == number) {
      vnode_hold(vno);
      mutex_unlock(&vcache_mtx);
      return vno;
    } 
  }
  mutex_unlock(&vcache_mtx);
  return NULL;
}

int vcache_add(struct fs *fs, struct vnode *vno) {
  mutex_lock(&vcache_mtx);
  vno_t i, can_free = VNO_INVALID;
  for(i=0; i<NVCACHE; i++) {
    if(vcache[i] == NULL)
      break;
    else if(vcache[i]->ref == 0)
      can_free = i;
  }
  if(i != NVCACHE) {
    vcache[i] = vno;
    list_pushfront(&vno->fs_link, &fs->vnode_list);
    mutex_unlock(&vcache_mtx);
    return 0;
  } else if(can_free != VNO_INVALID) {
    if(vcache[can_free]->ops->vsync)
      vcache[can_free]->ops->vsync(vcache[can_free]);

    list_remove(&vno->fs_link);

    if(vcache[can_free]->ops->vfree)
      vcache[can_free]->ops->vfree(vcache[can_free]);
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
  for(int i=0; i<NVCACHE; i++) {
    if(vcache[i] == vno) {
      vcache[i] = NULL;
      list_remove(&vno->fs_link);
      mutex_unlock(&vcache_mtx);
      return; 
    }
  }
  mutex_unlock(&vcache_mtx);
}

int fs_mountroot(const char *name, devno_t devno) {
  printf("fs: mounting rootfs(fstype=%s, devno=0x%x) ...\n", name, devno);
  if(blkdev_open(devno)) {
    puts("fs: failed to open device");
    return -1;
  }

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

/*
           parent retval fname
found       yes    yes    yes
not found   yes    no     yes
error       no     no     no
*/
static struct vnode *name_to_vnode(const char *path, struct vnode **parent, char **fname) {
  static char name[MAX_FILE_NAME+1];
  const char *ptr = path;
  struct vnode *prevvno = NULL;
  struct vnode *curvno = rootdir;

  if(curvno == NULL)
    return NULL;
  vnode_hold(curvno);
  vnode_lock(curvno);

  //currently, relative path is not allowed
  if(*ptr != '/')
    goto err;

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
      goto err;
    else
      name[i] = '\0';

printf("current name is %s\n", name);
    if(prevvno != NULL) {
      vnode_unlock(prevvno);
      vnode_release(prevvno);
    }

    prevvno = curvno;
    if(!curvno || !curvno->ops->lookup ||
      curvno->ops->lookup(curvno, name, &curvno) == LOOKUP_ERROR)
        goto err;
  }

err:
  if(curvno != NULL) {
    vnode_unlock(curvno);
    vnode_release(curvno);
  }
  
  if(prevvno != NULL) {
    vnode_unlock(prevvno);
    vnode_release(prevvno);
  }

  if(parent != NULL)
    *parent = NULL;
  if(fname != NULL)
    *fname = NULL;

  return NULL;
}

struct file *open(const char *path, int flags) {
  struct file *f;
  struct vnode *parent = NULL;
  char *name = NULL;

  flags += 1;
  if(!(flags & (_FREAD | _FWRITE)))
    return NULL;

  struct vnode *vno = name_to_vnode(path, &parent, &name);

  if(vno == NULL) {
    if(flags & _FCREAT) {
      if(parent && parent->ops->mknod) {
        if(parent->ops->mknod(parent, name, S_IFREG, 0))
          goto err;
      } else {
        goto err;
      }
    } else {
      goto err;
    }
  } else {
    if(flags & _FEXCL)
      goto err;
  }

  vnode_unlock(vno);
  vnode_unlock(parent);
  vnode_release(parent);
  if(name != NULL)
    free(name);

  f = file_new(vno, vno->file_ops, flags);

  if(flags & _FAPPEND)
    lseek(f, 0, SEEK_END);

  if(flags & _FTRUNC)
    truncate(f, 0);

  return f;

err:
  if(parent != NULL) {
    vnode_unlock(parent);
    vnode_release(parent);
  }
  if(vno != NULL) {
    vnode_unlock(vno);
    vnode_release(vno);
  }
  if(name != NULL)
    free(name);
  return NULL;
}

int mknod(const char *path, int mode, devno_t devno) {
  struct vnode *parent = NULL;
  char *name = NULL;
  struct vnode *vno = name_to_vnode(path, &parent, &name);
  int ret = -1;
  if(vno != NULL)
    goto err;
  if(!parent || !parent->ops->mknod)
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
  if(name != NULL)
    free(name);
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

  if(!parent || parent->fs != vno0->fs)
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
  if(!vno || !parent)
    goto err;
  if(!vno->ops->unlink)
    goto err;

  vnode_unlock(vno);
  vnode_release(vno);
  ret = parent->ops->unlink(parent, name, vno);

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
  struct vnode *vno = name_to_vnode(path, NULL, NULL);
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
  return ret;
}
