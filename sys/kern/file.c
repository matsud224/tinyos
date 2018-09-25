#include <kern/file.h>
#include <kern/lock.h>
#include <kern/kernlib.h>
#include <kern/fs.h>
#include <kern/thread.h>
#include <kern/syscalls.h>
#include <kern/chardev.h>

static struct list_head file_list;

static mutex filelist_mtx;

int is_invalid_fd(int fd) {
  return (fd < 0) || (fd >= MAX_FILES) || (current->files[fd] == NULL);
}

int fd_get() {
  for(int i=0; i<MAX_FILES; i++)
    if(current->files[i] == NULL)
      return i;
  return -1;
}

void file_init() {
  list_init(&file_list);

  mutex_init(&filelist_mtx);
}

struct file *file_new(void *data, const struct file_ops *ops, int type, int flags) {
  struct file *f = malloc(sizeof(struct file));
  bzero(f, sizeof(struct file));
  f->ref = 1;
  f->data = data;
  f->ops = ops;
  f->type = type;
  f->flags = flags;
  f->offset = 0;
  mutex_init(&f->mtx);
  if(f->ops->open) {
    if(f->ops->open(f, flags) != 0) {
      free(f);
      return NULL;
    }
  }
  return f;
}

struct file *dup(struct file *f) {
  mutex_lock(&f->mtx);
  f->ref++;
  mutex_unlock(&f->mtx);
  return f;
}

int close(struct file *f) {
  int retval = 0;
  mutex_lock(&f->mtx);
  if(f->ops->close)
    retval = f->ops->close(f);

  if(retval) {
    mutex_unlock(&f->mtx);
    return retval;
  }

  //mutex_lock(&filelist_mtx);
  //list_remove(&f->link);
  //mutex_unlock(&filelist_mtx);

  int ref = --(f->ref);
  mutex_unlock(&f->mtx);

  if(ref == 0) {
    free(f);
  }

  return retval;
}

int read(struct file *f, void *buf, size_t count) {
  mutex_lock(&f->mtx);
  if(f->ops->read) {
    int ret = f->ops->read(f, buf, count);
    mutex_unlock(&f->mtx);
    return ret;
  } else {
    mutex_unlock(&f->mtx);
    return 0;
  }
}

int write(struct file *f, const void *buf, size_t count) {
  mutex_lock(&f->mtx);
  if(f->ops->write) {
    int ret = f->ops->write(f, buf, count);
    mutex_unlock(&f->mtx);
    return ret;
  } else {
    mutex_unlock(&f->mtx);
    return 0;
  }
}

int lseek(struct file *f, off_t offset, int whence) {
  mutex_lock(&f->mtx);
  if(f->ops->lseek) {
    int ret = f->ops->lseek(f, offset, whence);
    mutex_unlock(&f->mtx);
    return ret;
  } else {
    mutex_unlock(&f->mtx);
    return -1;
  }
}

int fsync(struct file *f) {
  mutex_lock(&f->mtx);
  if(f->ops->sync) {
    int ret = f->ops->sync(f);
    mutex_unlock(&f->mtx);
    return ret;
  } else {
    mutex_unlock(&f->mtx);
    return -1;
  }
}

int truncate(struct file *f, size_t size) {
  mutex_lock(&f->mtx);
  if(f->ops->truncate) {
    int ret = f->ops->truncate(f, size);
    mutex_unlock(&f->mtx);
    return ret;
  } else {
    mutex_unlock(&f->mtx);
    return -1;
  }
}

int getdents(struct file *f, struct dirent *dirp, size_t count) {
  mutex_lock(&f->mtx);
  if(f->ops->getdents) {
    int ret = f->ops->getdents(f, dirp, count);
    mutex_unlock(&f->mtx);
    return ret;
  } else {
    mutex_unlock(&f->mtx);
    return -1;
  }
}

int sys_close(int fd) {
  if(is_invalid_fd(fd))
    return -1;
  int result = close(current->files[fd]);
  current->files[fd] = NULL;
  return result;
}

int sys_read(int fd, void *buf, size_t count) {
  if(is_invalid_fd(fd) || buffer_check(buf, count))
    return -1;
  return read(current->files[fd], buf, count);
}

int sys_write(int fd, const void *buf, size_t count) {
  if(is_invalid_fd(fd) || buffer_check(buf, count))
    return -1;
  return write(current->files[fd], buf, count);
}

int sys_isatty(int fd) {
  if(is_invalid_fd(fd))
    return -1;
  return (current->files[fd]->ops == &chardev_file_ops); //TODO
}

int sys_lseek(int fd, off_t offset, int whence) {
  if(is_invalid_fd(fd))
    return -1;
  return lseek(current->files[fd], offset, whence);
}

int sys_fsync(int fd) {
  if(is_invalid_fd(fd))
    return -1;
  return fsync(current->files[fd]);
}

int sys_truncate(int fd, size_t size) {
  if(is_invalid_fd(fd))
    return -1;
  return truncate(current->files[fd], size);
}

int sys_getdents(int fd, struct dirent *dirp, size_t count) {
  if(is_invalid_fd(fd) || buffer_check(dirp, count))
    return -1;
  return getdents(current->files[fd], dirp, count);
}

int sys_dup(int oldfd) {
  if(is_invalid_fd(oldfd))
    return -1;
  int newfd = fd_get();
  if(newfd < 0)
    return -1;
  current->files[newfd] = dup(current->files[oldfd]);
  if(current->files[newfd] == NULL)
    return -1;
  return newfd;
}
