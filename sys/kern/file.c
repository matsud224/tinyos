#include <kern/file.h>
#include <kern/lock.h>
#include <kern/kernlib.h>
#include <kern/fs.h>

static struct list_head file_list;

static mutex filelist_mtx;

void file_init() {
  list_init(&file_list);

  mutex_init(&filelist_mtx);
}

struct file *file_new(void *data, const struct file_ops *ops, int flags) {
  struct file *f = malloc(sizeof(struct file));
  bzero(f, sizeof(struct file));
  f->ref = 1;
  f->data = data;
  f->ops = ops;
  f->flags = flags;
  f->offset = 0;
  if(f->ops->open) {
    if(f->ops->open(f, flags) != 0) {
      free(f);
      return NULL;
    }
  }
  return f;
}

int close(struct file *f) {
  int retval = 0;
  if(f->ops->close)
    retval = f->ops->close(f);

  if(retval)
    return retval;

  mutex_lock(&filelist_mtx);
  //list_remove(&f->link);
  mutex_unlock(&filelist_mtx);

  free(f);

  return retval;
}

int read(struct file *f, void *buf, size_t count) {
  if(f->ops->read)
    return f->ops->read(f, buf, count);
  else
    return 0;
}

int write(struct file *f, const void *buf, size_t count) {
  if(f->ops->write)
    return f->ops->write(f, buf, count);
  else
    return 0;
}

int lseek(struct file *f, off_t offset, int whence) {
  if(f->ops->lseek)
    return f->ops->lseek(f, offset, whence);
  else
    return -1;
}

int sync(struct file *f) {
  if(f->ops->sync)
    return f->ops->sync(f);
  else
    return -1;
}

int truncate(struct file *f, size_t size) {
  if(f->ops->truncate)
    return f->ops->truncate(f, size);
  else
    return -1;
}

int getdents(struct file *f, struct dirent *dirp, size_t count) {
  if(f->ops->getdents)
    return f->ops->getdents(f, dirp, count);
  else
    return -1;
}

