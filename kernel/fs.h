#pragma once

#include <stddef.h>
#include <stdint.h>

struct inode {
  struct fs *fs;
  struct inode_ops *ops;
  uint32_t inode_no;
  uint32_t mode;
  uint32_t size;
};

#define INODE_DIR 0x4000

#define DENTOP_GET 0
#define DENTOP_CREATE 1
#define DENTOP_REMOVE 2

struct inode_ops {
  int (*read)(struct inode *inode, uint8_t *base, uint32_t offset, uint32_t count);
  int (*write)(struct inode *inode, uint8_t *base, uint32_t offset, uint32_t count);
  void (*resize)(struct inode *inode, uint32_t newsize);
  struct inode *(*opdent)(struct inode *inode, const char *name, int op);
};

struct fs_ops {
  struct inode *(*getroot)(struct fs *fs);
};

struct fs {
  struct fs_ops *ops;
};

struct fsinfo_ops {
  struct fs *(*mount)(void *source);
};

struct fsinfo {
  char name[32];
  struct fsinfo_ops *ops;
};


void fsinfo_add(struct fsinfo *info);
int fs_mountroot(const char *name, void *source);
struct inode *fs_nametoi(const char *path);
int fs_read(struct inode *inode, uint8_t *base, uint32_t offset, uint32_t count);
