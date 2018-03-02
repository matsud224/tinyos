#pragma once
#include <kern/kernlib.h>

#define CDBUF_IS_EMPTY(b) ((b)->size == (b)->free)
#define CDBUF_IS_FULL(b) ((b)->free == 0)

struct chardev_buf {
  size_t size;
  u32 free;
  u32 head; //次の書き込み位置
  u32 tail; //次の読み出し位置
  u8 *addr;
};

struct chardev;

struct chardev_ops {
  int (*open)(int minor);
  int (*close)(int minor);
  u32 (*read)(int minor, u8 *dest, size_t count);
  u32 (*write)(int minor, u8 *src, size_t count);
};

void chardev_init(void);
void chardev_add(struct chardev *dev);
struct chardev_buf *cdbuf_create(u8 *mem, size_t size);
u32 cdbuf_read(struct chardev_buf *buf, u8 *dest, size_t count);
u32 cdbuf_write(struct chardev_buf *buf, u8 *src, size_t count);
u32 chardev_open(devno_t devno);
u32 chardev_close(devno_t devno);
u32 chardev_read(devno_t devno, u8 *dest, size_t count);
u32 chardev_write(devno_t devno, u8 *src, size_t count);


