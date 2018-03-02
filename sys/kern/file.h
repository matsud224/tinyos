#pragma once
#include <kern/kernlib.h>

#define FILE_REGULAR	0
#define FILE_DEVICE		1
#define FILE_PIPE			2
#define FILE_SOCKET		3

struct file_ops;

struct file {
  struct list_head link;
  int type;
  struct file_ops *ops;
  int ref;
  void *fcb;
};

struct file_ops {
  int (*read)(void *fcb, void *buf, size_t count);
  int (*write)(void *fcb, const void *buf, size_t count);
  int (*lseek)(void *fcb, off_t offset, int whence);
  int (*close)(void *fcb);
  int (*sync)(void *fcb);
};

struct file file_new(void *fcb, struct file_ops *ops);
struct file *open(const char *path);
int read(struct file *f, void *buf, size_t count);
int write(struct file *f, const void *buf, size_t count);
int lseek(struct file *f, off_t offset, int whence);
int close(struct file *f);
int mknod(struct file *f, int mode, dev_t dev);
int link(const char *oldpath, const char *newpath);
int unlink(const char *path);
int stat(const char *path);

