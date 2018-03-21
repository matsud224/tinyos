#pragma once
#include <kern/kernlib.h>
#include <kern/fs.h>

#define	_FREAD		0x0001	/* read enabled */
#define	_FWRITE		0x0002	/* write enabled */
#define	_FAPPEND	0x0008	/* append (writes guaranteed at the end) */
#define	_FCREAT		0x0200	/* open with file create */
#define	_FTRUNC		0x0400	/* open with truncation */
#define	_FEXCL		0x0800	/* error on open if file exists */
#define _FDIRECTORY     0x200000

#define	O_RDONLY	0		/* +1 == FREAD */
#define	O_WRONLY	1		/* +1 == FWRITE */
#define	O_RDWR		2		/* +1 == FREAD|FWRITE */
#define	O_APPEND				_FAPPEND
#define	O_CREAT					_FCREAT
#define	O_TRUNC					_FTRUNC
#define	O_EXCL					_FEXCL
#define O_SYNC					_FSYNC
#define O_DIRECTORY     _FDIRECTORY

struct file_ops;

struct file {
  struct list_head link;
  const struct file_ops *ops;
  int ref;
  void *data;
  off_t offset;
  int flags;
};

struct file_ops {
  int (*open)(struct file *f, int flags);
  int (*read)(struct file *f, void *buf, size_t count);
  int (*write)(struct file *f, const void *buf, size_t count);
  int (*lseek)(struct file *f, off_t offset, int whence);
  int (*close)(struct file *f);
  int (*sync)(struct file *f);
  int (*truncate)(struct file *f, size_t size);
  int (*getdents)(struct file *f, struct dirent *dirp, size_t count);
};

enum seek_whence {
  SEEK_SET,
  SEEK_CUR,
  SEEK_END,
};

struct file *file_new(void *data, const struct file_ops *ops, int flags);
int read(struct file *f, void *buf, size_t count);
int write(struct file *f, const void *buf, size_t count);
int lseek(struct file *f, off_t offset, int whence);
int close(struct file *f);
int sync(struct file *f);
int truncate(struct file *f, size_t size);
int getdents(struct file *f, struct dirent *dirp, size_t count);

