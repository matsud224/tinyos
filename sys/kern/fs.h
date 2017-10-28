#pragma once
#include <kern/kernlib.h>

struct inode {
  struct fs *fs;
  const struct inode_ops *ops;
  u32 inode_no;
  u32 mode;
  size_t size;
};

#define INODE_DIR 0x4000

enum dent_op {
  DENTOP_GET		= 0,
  DENTOP_CREATE	= 1,
  DENTOP_REMOVE	= 2,
};

struct inode_ops {
  int (*read)(struct inode *inode, u8 *base, u32 offset, size_t count);
  int (*write)(struct inode *inode, u8 *base, u32 offset, size_t count);
  void (*resize)(struct inode *inode, u32 newsize);
  struct inode *(*opdent)(struct inode *inode, const char *name, int op);
};

struct fs_ops {
  struct inode *(*getroot)(struct fs *fs);
};

struct fs {
  const struct fs_ops *ops;
};

struct fsinfo_ops {
  struct fs *(*mount)(void *source);
};

struct fsinfo {
  char name[32];
  const struct fsinfo_ops *ops;
};


void fsinfo_add(const struct fsinfo *info);
int fs_mountroot(const char *name, void *source);
struct inode *fs_nametoi(const char *path);
int fs_read(struct inode *inode, u8 *base, u32 offset, size_t count);
