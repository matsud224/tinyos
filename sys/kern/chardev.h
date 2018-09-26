#pragma once
#include <kern/kernlib.h>
#include <kern/file.h>

#define CDBUF_IS_EMPTY(b) ((b)->size == (b)->free)
#define CDBUF_IS_FULL(b) ((b)->free == 0)

struct chardev_buf {
  size_t size;
  size_t free;
  size_t head; //次の書き込み位置
  size_t tail; //次の読み出し位置
  char *addr;
};

#define CDMODE_CANON   0x1
#define CDMODE_ECHO    0x2
struct chardev_state {
  int mode;
  char linebuf[MAX_CHARDEV_LINE_CHARS + 1];
#define CDTEMPBUFSIZE 512
  char tempbuf[CDTEMPBUFSIZE];
  int linebuf_head;
};

struct chardev_ops {
  int (*open)(int minor);
  int (*close)(int minor);
  int (*read)(int minor, char *dest, size_t count);
  int (*write)(int minor, const char *src, size_t count);
  struct chardev_state *(*getstate)(int minor);
};

extern const struct file_ops chardev_file_ops;

void chardev_init(void);
int chardev_register(const struct chardev_ops *ops);
struct chardev_buf *cdbuf_create(char *mem, size_t size);
int cdbuf_read(struct chardev_buf *buf, char *dest, size_t count);
int cdbuf_write(struct chardev_buf *buf, const char *src, size_t count);
int chardev_open(devno_t devno);
int chardev_close(devno_t devno);
int chardev_read(devno_t devno, char *dest, size_t count);
int chardev_write(devno_t devno, const char *src, size_t count);
void chardev_initstate(struct chardev_state *state, int mode);

