#include <kern/file.h>
#include <kern/lock.h>
#include <kern/kernlib.h>

static struct list_head file_list;

static mutex filelist_mtx;

FILE_INIT void file_init() {
  list_init(&file_list);

  mutex_init(&filelist_mtx);
}

struct file file_new(int type, void *fcb, struct file_ops *ops) {
  struct file *f = malloc(sizeof(struct file));
  f->type = type;
  f->ref = 1;
  f->fcb = fcb;
  f->ops = ops;
  return f;
}

int close(struct file *f) {
  int retval = f->ops->close(f);

  mutex_lock(&filelist_mtx);
  list_remove(&f->link);
  mutex_unlock(&filelist_mtx);

  return retval;
}

int read(struct file *f, void *buf, size_t count) {
  return f->ops->read(f->fcb, buf, count);
}

int write(struct file *f, const void *buf, size_t count) {
  return f->ops->write(f->fcb, buf count);
}

int lseek(struct file *f, off_t offset, int whence) {
  return f->ops->lseek(f->fcb, offset, whence);
}

