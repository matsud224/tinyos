#pragma once
#include <kern/kernlib.h>

struct file_ops;

struct file {
  struct list_head link;
  const struct file_ops *ops;
  int ref;
  void *data;
  off_t offset;
};

struct file_ops {
  int (*open)(struct file *f);
  int (*read)(struct file *f, void *buf, size_t count);
  int (*write)(struct file *f, const void *buf, size_t count);
  int (*lseek)(struct file *f, off_t offset, int whence);
  int (*close)(struct file *f);
  int (*sync)(struct file *f);
};

enum seek_whence {
  SEEK_SET,
  SEEK_CUR,
  SEEK_END,
};

struct file *file_new(void *data, const struct file_ops *ops);
int read(struct file *f, void *buf, size_t count);
int write(struct file *f, const void *buf, size_t count);
int lseek(struct file *f, off_t offset, int whence);
int close(struct file *f);

